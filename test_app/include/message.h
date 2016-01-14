#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <stdint.h>

typedef struct Message {
    unsigned int mstype:8;
    unsigned int inst:16;
    unsigned int rnd:8;
    unsigned int vrnd:8;
    uint64_t acpid;
    char value[64];
} Message;

void message_to_string(Message m, char *str);
#endif