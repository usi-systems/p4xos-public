#ifndef _MESSAGE_H_
#define _MESSAGE_H_

typedef struct Message {
    char mstype;
    short inst;
    char rnd;
    char vrnd;
    char acpid[8];
    char value[64];
} Message;

void message_to_string(Message m, char *str);
#endif