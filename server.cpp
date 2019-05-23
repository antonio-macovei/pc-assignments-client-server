#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>

#include "helper.h"

using namespace std;

void usage(char *file) {
    cerr << "Usage: " << file << " <PORT>\n";
    exit(0);
}

void cmd_usage() {
    cerr << "Available commands are:\n\texit\n";
}

void exit_if(bool assertion, string message) {
    if (assertion) {
        cerr << "Error: " << message << "\n";
        exit(0);
    }
}

void print_if(bool assertion, string message) {
    if (assertion) {
        cerr << "Error: " << message << "\n";
    }
}

int find_client_by_fd(int fd, vector<tcp_client> tcp_clients) {
    // Get the index of the client
    for (int i = 0; i < tcp_clients.size(); i++) {
        if (tcp_clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

int find_topic_by_name(string topic, vector<topic_struct> topics) {
    // Get the index of the topic
    for (int i = 0; i < topics.size(); i++) {
        if (topics[i].name.compare(topic) == 0) {
            return i;
        }
    }
    return -1;
}

int find_stored_message(extended_message msg, vector<stored_message> stored_messages) {
    // Get the index of the stored message
    for (int i = 0; i < stored_messages.size(); i++) {
        if (strcmp((char*)stored_messages[i].msg.msg.payload, (char*)msg.msg.payload) == 0 &&
            strcmp(stored_messages[i].msg.msg.topic, msg.msg.topic) == 0 &&
            stored_messages[i].msg.msg.type == msg.msg.type) {
            return i;
        }
    }
    return -1;
}

bool is_subscribed(tcp_client client, topic_struct topic) {
    // Check if the client is subscribed to the current topic
    for (auto s : client.subscriptions) {
        if (s.first == topic.id) {
            return true;
        }
    }
    return false;
}

bool is_sf_activated(tcp_client client, topic_struct topic) {
    // Check if the SF flag is activated for current topic
    for (auto s : client.subscriptions) {
        if (s.first == topic.id) {
            return s.second;
        }
    }
    return false;
}

void subscribe_client(int fd, string s_topic, bool sf,
                                vector<tcp_client> &tcp_clients,
                                vector<topic_struct> topics) {

    // Get current client and current topic
    int client_index = find_client_by_fd(fd, tcp_clients);
    int topic_index = find_topic_by_name(s_topic, topics);
    tcp_client client = tcp_clients[client_index];
    topic_struct topic = topics[topic_index];

    // Iterate through the client's subscriptions and update the SF flag if found
    vector<pair<int, bool> > subscriptions = client.subscriptions;
    bool found = false;
    for (int i = 0; i < subscriptions.size(); i++) {
        if (subscriptions[i].first == topic.id) {
            subscriptions[i].second = sf;
            found = true;
            break;
        }
    }

    if (!found) {
        // Subscription not found, add it to the vector
        subscriptions.push_back({topic.id, sf});
    }

    client.subscriptions = subscriptions;
    tcp_clients[client_index] = client;
}

void unsubscribe_client(int fd, string s_topic,
                                    vector<tcp_client> &tcp_clients,
                                    vector<topic_struct> topics) {

    // Get current client and current topic
    int client_index = find_client_by_fd(fd, tcp_clients);
    int topic_index = find_topic_by_name(s_topic, topics);
    tcp_client client = tcp_clients[client_index];
    topic_struct topic = topics[topic_index];

    // Iterate through the client's subscriptions and get the one to the current topic
    vector<pair<int, bool> > subscriptions = client.subscriptions;
    pair<int, bool>* existing_subscription = nullptr;
    for (auto s : subscriptions) {
        if (s.first == topic.id) {
            existing_subscription = &s;
            break;
        }
    }

    if (existing_subscription != nullptr) {
        // Subscription exists, remove it and shift all the elements to the left
        bool found = false;
        for (int i = 0; i < subscriptions.size(); i++) {
            if (found) {
                subscriptions[i - 1] = subscriptions[i];
            }

            if (!found && subscriptions[i].first == topic.id) {
                found = true;
            }
        }

        // Resize the vector
        subscriptions.resize(subscriptions.size() - 1);
    }

    client.subscriptions = subscriptions;
    tcp_clients[client_index] = client;
}

void send_to_subscribers(message msg, sockaddr_in client_addr, fd_set fds,
                                        int &stored_count, vector<tcp_client> &tcp_clients,
                                        vector<topic_struct> &topics,
                                        vector<stored_message> &stored_messages) {

    // Prepare the message to be sent
    extended_message ext_msg;
    memset(&ext_msg, 0, sizeof(extended_message));
    ext_msg.port = ntohs(client_addr.sin_port);
    strcpy(ext_msg.ip, inet_ntoa(client_addr.sin_addr));
    memcpy(&ext_msg.msg, &msg, sizeof(message));

    // Find the current topic
    int topic_index = find_topic_by_name(msg.topic, topics);
    topic_struct topic = topics[topic_index];

    // Iterate through all the clients and send the message to those subscribed
    // to the current topic
    for (int i = 0; i < tcp_clients.size(); i++) {
        tcp_client client = tcp_clients[i];
        vector<pair<int, bool> > subscriptions = client.subscriptions;
        if (is_subscribed(client, topic)) {
            // Client subscribed to the current topic
            if (FD_ISSET(client.fd, &fds)) {
                // Client connected, send the message
                int n = send(client.fd, &ext_msg, sizeof(extended_message), 0);
                print_if(n < 0, "An error occurred while sending the data to the client!");
            } else if (is_sf_activated(client, topic)) {
                // Client not connected, store the message if SF is activated
                int stored_id = find_stored_message(ext_msg, stored_messages);
                if (stored_id == -1) {
                    // Not found, add it to collection
                    stored_message stored_msg;
                    stored_msg.id = stored_id = stored_count;
                    memcpy(&stored_msg.msg, &ext_msg, sizeof(extended_message));
                    stored_msg.pending_clients = 1;

                    stored_messages.push_back(stored_msg);
                    stored_count++;

                } else {
                    // Found, increase the pending_clients number
                    stored_messages[stored_id].pending_clients++;
                }

                client.stored_messages.push_back(stored_id);
                tcp_clients[i] = client;
            }
        }
    }
}

void remove_stored_message(extended_message ext_msg,
                                                vector<stored_message> &stored_messages) {
    int stored_id = find_stored_message(ext_msg, stored_messages);
    stored_messages.erase(stored_messages.begin() + stored_id);
}

void resend_to_subscriber(tcp_client &client, vector<stored_message> &stored_messages) {
    for (auto message_id : client.stored_messages) {
        extended_message ext_msg;
        memset(&ext_msg, 0, sizeof(extended_message));
        memcpy(&ext_msg, &stored_messages[message_id].msg, sizeof(extended_message));

        int n = send(client.fd, &ext_msg, sizeof(extended_message), 0);
        print_if(n < 0, "An error occurred while sending the data to the client!");

        stored_messages[message_id].pending_clients--;

        if (stored_messages[message_id].pending_clients == 0) {
            // If there are no more client waiting to receive this message, remove it
            remove_stored_message(ext_msg, stored_messages);
        }
    }
    // Clear the stored messages for this client
    client.stored_messages.clear();
}

void add_new_topic(char* c_topic, int &topics_count, vector<topic_struct> &topics) {
    string topic(c_topic, min(TOPIC_LEN, (int)strlen(c_topic)));

    // Check if topic already exists
    for (auto t : topics) {
        if (t.name.compare(topic) == 0) {
            return;
        }
    }

    // Create new topic
    topic_struct new_topic;
    new_topic.name = topic;
    new_topic.id = topics_count;

    // Increment the topic ID
    topics_count++;

    topics.push_back(new_topic);
}

void parse_cmd(int listen_fd, int udp_fd, string line, vector<tcp_client> tcp_clients) {
    if (line.compare(EXIT_STR) == 0) {
        // Send a message to all the clients to close the connection
        for (auto client : tcp_clients) {
            extended_message ext_msg;
            memset(&ext_msg, 0, sizeof(extended_message));
            strncpy((char *)ext_msg.msg.payload, EXIT_STR, strlen(EXIT_STR));
            int n = send(client.fd, &ext_msg, sizeof(extended_message), 0);
            print_if(n < 0, "An error occurred while sending the data to the client!");

            // Close the connection with each of them
            close(client.fd);
        }

        // Close the TCP and UDP sockets
        close(listen_fd);
        close(udp_fd);
        exit(0);

    } else {
        // Invalid command
        cmd_usage();
        return;
    }
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        usage(argv[0]);
    }

    int port = atoi(argv[1]);
    exit_if(port == 0, "<PORT> must be an integer!");

    int listen_fd, udp_fd, new_client_fd;
    int n, ret, flag, topics_count = 0, stored_count = 0;
    char new_client_id[MAX_CLIENT_ID_LEN];
    struct sockaddr_in server_addr, client_addr;

    vector<tcp_client> tcp_clients;
    vector<stored_message> stored_messages;
    vector<topic_struct> topics;

    fd_set read_fds; // File descriptors array used in select()
    fd_set tmp_fds; // Temporary file descriptors array
    int fd_max = 0; // Maximum value for fd in read_fds

    // Set server_addr fields
    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Empties the file descriptors array for reading (read_fds) and for temporary storage (tmp_fds)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // Create a socket for TCP listening
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    exit_if(listen_fd < 0, "An error occurred while opening a TCP socket connection!");

    // Create a socket for UDP listening
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0); 
    exit_if(udp_fd < 0, "An error occurred while opening an UDP socket connection!");

    // Socket options -> activate TCP_NODELAY to disable Neagle buffering
    flag = 1;
    ret = setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    print_if(ret < 0, "An error occurred while applying TCP settings!");

    // Bind the TCP listening socket
    ret = bind(listen_fd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
    exit_if(ret < 0, "An error occurred while binding TCP!");

    // Bind the UDP listening socket
    ret = bind(udp_fd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
    exit_if(ret < 0, "An error occurred while binding UDP!");

    // Start listening on the TCP socket
    ret = listen(listen_fd, MAX_QUEUE);
    exit_if(ret < 0, "An error occurred while trying to listen on TCP!");

    // Add the new file descriptors to read_fds array
    FD_SET(listen_fd, &read_fds);
    FD_SET(udp_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    // The max value of FD
    fd_max = max(listen_fd, udp_fd);

    while (true) {
        tmp_fds = read_fds;

        // Select the ready file descriptor
        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
        print_if(ret < 0, "An error occurred while selecting a connection!");

        // Iterate through all the file descriptors to find the one that is ready
        // and act accordingly to its type
        for (int fd = 0; fd <= fd_max; fd++) {
            if (!FD_ISSET(fd, &tmp_fds))  continue;

            if (fd == STDIN_FILENO) {
                // Incoming input from the STDIN
                string line;
                getline(std::cin, line);
                parse_cmd(listen_fd, udp_fd, line, tcp_clients);

            } else if (fd == listen_fd) {
                // Incoming connection on the TCP socket
                socklen_t client_len = sizeof(client_addr);
                new_client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);
                print_if(new_client_fd < 0, "An error occurred while accepting the TCP connection!");

                // Wait for the CLIENT_ID to be received
                memset(new_client_id, 0, sizeof(new_client_id));
                n = recv(new_client_fd, new_client_id, sizeof(new_client_id), 0);
                print_if(n < 0, "An error occured while receiving the CLIENT_ID!");
                if (n < 0) continue;

                // Add the new client to the clients list or update existing entry
                int client_index = find_client_by_fd(new_client_fd, tcp_clients);
                if (client_index != -1) {
                    // Existing client
                    tcp_client existing_client = tcp_clients[client_index];
                    existing_client.fd = new_client_fd;
                    tcp_clients[client_index] = existing_client;
                    resend_to_subscriber(existing_client, stored_messages);
                    tcp_clients[client_index] = existing_client;
                } else {
                    // New client
                    tcp_client new_client;
                    new_client.client_id = new_client_id;
                    new_client.fd = new_client_fd;
                    tcp_clients.push_back(new_client);
                }

                // Add the new socket to the read_fds array
                FD_SET(new_client_fd, &read_fds);

                // Update the fd_max to the new socket value
                if (new_client_fd > fd_max) {
                    fd_max = new_client_fd;
                }

                cout << "New client (" << new_client_id << ") connected from "
                        << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) <<".\n";

            } else if (fd == udp_fd) {
                // Incoming connection on the UDP socket
                message msg;
                memset(&msg, 0, sizeof(message));
                flag = 0;
                socklen_t addr_len = sizeof(client_addr);

                // Receive the message from UDP client
                n = recvfrom(udp_fd, &msg, sizeof(message), flag,
                                        (struct sockaddr *) &client_addr, &addr_len);
                print_if(n < 0, "An error occured while receiving data from an UDP client!");

                // Send the message to the subscribers of the current topic
                add_new_topic(msg.topic, topics_count, topics);
                send_to_subscribers(msg, client_addr, read_fds, stored_count,
                                                    tcp_clients, topics, stored_messages);

            } else {
                // Incoming command from TCP connection
                command cmd;
                memset(&cmd, 0, sizeof(command));
                n = recv(fd, &cmd, sizeof(command), 0);
                print_if(n < 0, "An error occured while receiving a command from a TCP client!");

                if (n == 0) {
                    // Close connection with this socket
                    int client_index = find_client_by_fd(fd, tcp_clients);
                    cout << "Client (" << tcp_clients[client_index].client_id << ") disconnected.\n";
                    close(fd);

                    // Remove the closed socket from the read_fds array
                    FD_CLR(fd, &read_fds);

                } else {
                    // New command received from the TCP client
                    if (strcmp(cmd.cmd, SUBSCRIBE_STR) == 0) {
                        add_new_topic(cmd.topic, topics_count, topics);
                        subscribe_client(fd, cmd.topic, cmd.sf, tcp_clients, topics);
                    } else if (strcmp(cmd.cmd, UNSUBSCRIBE_STR) == 0) {
                        if (find_topic_by_name(cmd.topic, topics) != -1) {
                            unsubscribe_client(fd, cmd.topic, tcp_clients, topics);
                        }
                    }
                }
            }
        }
    }

    return 0;
}