#define PAYLOAD_LEN 1500
#define TOPIC_LEN 50
#define CMD_LEN 12
#define MAX_CLIENT_ID_LEN 10

#define EXIT_STR "exit"
#define SUBSCRIBE_STR "subscribe"
#define UNSUBSCRIBE_STR "unsubscribe"
#define INT_STR "INT"
#define SHORT_REAL_STR "SHORT_REAL"
#define FLOAT_STR "FLOAT"
#define STRING_STR "STRING"

#define CMD_SUBSCRIBE 1
#define CMD_UNSUBSCRIBE 0

#define MAX_QUEUE 32

enum Types {INT, SHORT_REAL, FLOAT, STRING};

struct message {
    char topic[TOPIC_LEN];
    uint8_t type;
    unsigned char payload[PAYLOAD_LEN];
};

struct command {
    char topic[TOPIC_LEN];
    char cmd[CMD_LEN];
    bool sf;
};

struct extended_message {
    char ip[16];
    unsigned short port;
    message msg;
};

struct tcp_client {
    std::string client_id;
    int fd;
    std::vector<std::pair<int, bool> > subscriptions;
    std::vector<int> stored_messages;
};

struct stored_message {
    int id;
    extended_message msg;
    int pending_clients;
};

struct topic_struct {
    int id;
    std::string name;
};