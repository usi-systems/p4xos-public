#include <stdio.h>
#include <strings.h>
#include "message.h"

void message_to_string(Message m, char *str) {
    sprintf(str,
        "msgtype:  %c\n"
        "instance: %hu\n"
        "round:    %c\n"
        "vround:   %c\n"
        "acceptor: %.8s\n"
        "value:    %s\n",
        m.mstype, m.inst, m.rnd, m.vrnd, m.acpid, m.value);
}