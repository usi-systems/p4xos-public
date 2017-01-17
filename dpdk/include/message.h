#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

enum Operation {
    GET,
    SET
};

struct command {
    struct timespec ts;
    uint16_t command_id;
    enum Operation op;
    char content[32];
};


struct __attribute__((__packed__)) client_request {
    uint16_t length;
    struct sockaddr_in cliaddr;
    char content[1];
};

uint16_t content_length(struct client_request *request);
uint16_t message_length(struct client_request *request);
struct client_request* create_client_request(const char *data, uint16_t data_size);
void print_message(struct client_request *request);
void hexdump_message(struct client_request *request);
#endif