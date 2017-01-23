#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "message.h"

uint16_t content_length(struct client_request *request)
{
    return request->length - (sizeof(struct client_request) - 1);
}

uint16_t message_length(struct client_request *request)
{
    return request->length;
}

struct client_request* create_client_request(const char *data, uint16_t data_size)
{
    uint16_t message_size = sizeof(struct client_request) + data_size - 1;
    struct client_request *request = (struct client_request*)malloc(message_size);
    request->length = message_size;
    memcpy(request->content, data, data_size);
    return request;
}

void print_message(struct client_request *request)
{
    printf("Length %d\n", request->length);
    // printf("%ld.%.9ld\n", request->ts.tv_sec, request->ts.tv_nsec);
    printf("Content %s\n", request->content);
}

void hexdump_message(struct client_request *request)
{
    int i;
    char *data = (char *)request;
    for (i = 0; i < request->length; i++) {
        if (i % 16 ==0)
            printf("\n");
        printf("%02x ", (unsigned char)data[i]);
    }
    printf("\n");
}
