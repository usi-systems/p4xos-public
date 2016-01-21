#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <stdint.h>
#include <sys/time.h>

typedef struct Message {
    unsigned int mstype:8;
    unsigned int inst:16;
    unsigned int rnd:8;
    unsigned int vrnd:8;
    uint64_t acpid;
    struct timeval ts;
    char value[64];
} Message;

Message decode_message(Message m);
void message_to_string(Message m, char *str);

#endif