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
#include <math.h>

#include "helper.h"

using namespace std;

void usage(char *file) {
    fprintf(stderr, "Usage: %s <CLIENT_ID> <IP_SERVER> <PORT_SERVER>\n", file);
    exit(0);
}

void cmd_usage() {
    cerr << "Available commands are:\n\tsubscribe "
            << "<TOPIC> <SF>\n\tunsubscribe <TOPIC>\n\texit\n";
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

string decode_int(unsigned char *payload) {
    int n = 0;
    n = payload[1] << 24 |
        payload[2] << 16 |
        payload[3] << 8 |
        payload[4];

    if (payload[0] == 1) {
        n *= -1;
    }

    char c_str[20];
    sprintf(c_str, "%d", n);
    string s(c_str, strlen(c_str));
    return s;
}

string decode_short_real(unsigned char *payload) {
    unsigned short n = 0;
    n = (unsigned short)(payload[0] << 8 | payload[1]);

    char c_str[20];
    sprintf(c_str, "%u", n);
    int c_len = strlen(c_str) + 1;
    string s(c_str, c_len);

    for (int i = 0, p = c_len - 1; i < 2; p--, i++) {
        s[p] = s[p - 1];
    }
    s[c_len - 3] = '.';

    return s;
}

string decode_float(unsigned char *payload) {
    int n = 0;
    n = payload[1] << 24 |
        payload[2] << 16 |
        payload[3] << 8 |
        payload[4];

    if (payload[0] == 1) {
        n *= -1;
    }

    double d = n;
    d = d / pow(10, payload[5]);

    char c_str[20];
    sprintf(c_str, "%.10g", d);
    string s(c_str, strlen(c_str));
    return s;
}

string decode_string(unsigned char *payload) {
    return string((char *)payload);
}

void print_message(extended_message ext_msg) {
    // Print the IP, PORT and topic and decode the content of the message
    cout << ext_msg.ip << ":" << ext_msg.port << " - " << ext_msg.msg.topic << " - ";
    switch (ext_msg.msg.type) {
        case INT: {
            string n = decode_int(ext_msg.msg.payload);
            cout << INT_STR << " - " << n << "\n";
            break;
        }
        case SHORT_REAL: {
            string f = decode_short_real(ext_msg.msg.payload);
            cout << SHORT_REAL_STR << " - " << f << "\n";
            break;
        }
        case FLOAT: {
            string d = decode_float(ext_msg.msg.payload);
            cout << FLOAT_STR << " - " << d << "\n";
            break;
        }
        case STRING: {
            string s = decode_string(ext_msg.msg.payload);
            cout << STRING_STR << " - " << s << "\n";
            break;
        }
    }
}

void send_cmd(int cmd_type, int fd, string topic, int sf = 0) {
    command cmd;
    memset(&cmd, 0, sizeof(command));
    strncpy(cmd.topic, topic.c_str(), topic.length());

    // Prepare the cmd struct to be sent
    if (cmd_type == CMD_SUBSCRIBE) {
        strncpy(cmd.cmd, SUBSCRIBE_STR, sizeof(SUBSCRIBE_STR));
        cmd.sf = sf == 1 ? true : false;
    } else if (cmd_type == CMD_UNSUBSCRIBE) {
        strncpy(cmd.cmd, UNSUBSCRIBE_STR, sizeof(UNSUBSCRIBE_STR));
        cmd.sf = false;
    }

    // Send the command to the server
    int n = send(fd, &cmd, sizeof(command), 0);
    print_if(n < 0, "An error occurred while sending the command to the server!");
}

vector<string> parse_line(string cmd, string delimiter) {
    size_t pos = 0;
    string token;
    vector<string> result;

    // Split the line by delimiter and put the elements in result vector
    while ((pos = cmd.find(delimiter)) != string::npos) {
        token = cmd.substr(0, pos);
        result.push_back(token);
        cmd.erase(0, pos + delimiter.length());
    }
    result.push_back(cmd);

    return result;
}

void parse_cmd(int sock_fd, string line) {
    string cmd, topic, sf_string;
    int sf = 0;

    if (line.compare(EXIT_STR) == 0) {
        // Close the connection and exit
        close(sock_fd);
        exit(0);
    } else {
        // Parse the command and send it to the server
        vector<string> parsed_string = parse_line(line, " ");

        // Check the number of arguments (2 is the minimum for both cmds)
        if (parsed_string.size() < 2) {
            cmd_usage();
            return;
        }

        // Get the arguments
        cmd = parsed_string[0];
        topic = parsed_string[1];

        if (topic.length() > 50) {
            cmd_usage();
            return;
        }

        if (cmd.compare(SUBSCRIBE_STR) == 0) {
            // If the command is SUBSCRIBE, check for 3 arguments
            if (parsed_string.size() < 3) {
                cmd_usage();
                return;
            }

            // Get the SF flag as string and convert it to int
            sf_string = parsed_string[2];
            try {
                sf = stoi(sf_string);
            } catch(...) {
                cmd_usage();
            }

            if (sf != 0 && sf != 1) {
                print_if(sf != 0 && sf != 1, "<SF> must be an integer, 0 or 1!");
                return;
            }

            send_cmd(CMD_SUBSCRIBE, sock_fd, topic, sf);
            cout << "subscribed " << topic << "\n";
        } else if (cmd.compare(UNSUBSCRIBE_STR) == 0) {
            send_cmd(CMD_UNSUBSCRIBE, sock_fd, topic);
            cout << "unsubscribed " << topic << "\n";
        } else {
            cmd_usage();
            return;
        }
    }
}

int main(int argc, char *argv[]) {

    if (argc < 4) {
        usage(argv[0]);
    }

    int port = atoi(argv[3]);
    exit_if(port == 0, "<PORT> must be an integer!");

    char client_id[10];
    memcpy(client_id, argv[1], sizeof(argv[1]));
    exit_if(strlen(client_id) > MAX_CLIENT_ID_LEN,
            "<CLIENT_ID> must be a string of maximum 10 characters!");

    int n, ret, flag, sock_fd;
    struct sockaddr_in server_addr;

    fd_set read_fds; // File descriptors array used in select()
    fd_set tmp_fds; // Temporary file descriptors array
    int fd_max = 0; // Maximum value for fd in read_fds

    // Set server_addr fields
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    ret = inet_aton(argv[2], &server_addr.sin_addr);
    exit_if(ret == 0, "<IP> must be a valid IP address!");

    // Create a TCP socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    exit_if(sock_fd < 0, "An error occurred while opening a TCP socket connection!");

    // Socket options -> activate TCP_NODELAY to disable Neagle buffering
    flag = 1;
    ret = setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    print_if(ret < 0, "An error occurred while applying TCP settings!");

    // Create a TCP connection to the server
    ret = connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    exit_if(ret < 0, "An error occurred while connecting to the server!");

    // Send the CLIENT_ID to the server
    n = send(sock_fd, client_id, sizeof(client_id), 0);
    exit_if(n < 0, "An error occurred while sending the CLIENT_ID to the server!");

    // Add the new file descriptors to read_fds array
    FD_SET(sock_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    // The max value of FD
    fd_max = sock_fd;

    while (1) {
        tmp_fds = read_fds;

        // Select the ready file descriptor
        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
        print_if(ret < 0, "An error occurred while selecting a connection!");

        // Iterate through all the file descriptors to find the one that is ready
        // and act accordingly to its type
        for (int fd = 0; fd <= fd_max; fd++) {
            if (!FD_ISSET(fd, &tmp_fds)) continue;

            if (fd == STDIN_FILENO) {
                // Incoming input from the STDIN
                string line;
                getline(std::cin, line);
                parse_cmd(sock_fd, line);
            } else {
                // Incoming message from the server
                extended_message ext_msg;
                memset(&ext_msg, 0, sizeof(extended_message));
                n = recv(fd, &ext_msg, sizeof(extended_message), 0);
                print_if(n < 0, "An error occurred while receiving data from the server!");

                if (strcmp((char *)ext_msg.msg.payload, EXIT_STR) == 0) {
                    // The server sent an EXIT command, close the socket
                    close(sock_fd);
                    exit(0);
                }

                // Print the decoded message
                print_message(ext_msg);
            }
        }
    }

    return 0;
}