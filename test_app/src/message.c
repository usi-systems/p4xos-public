#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/types.h>
#include "message.h"


uint64_t invert_byte_order(uint64_t m) {
        uint64_t i64;
        uint64_t lsb = m & 0xFFFFFFFF;
        uint64_t msb = m >> 32;
        i64 = ((lsb << 32) + msb);
        return i64;
}

void message_to_string(Message m, char *str) {
    sprintf(str,
        "msgtype:  %.2x\n"
        "instance: %hu\n"
        "round:    %.2x\n"
        "vround:   %.2x\n"
        "acceptor: %"PRIx64"\n"
        "time: %lld.%06ld\n"
        "value:    %s\n",
        m.mstype, m.inst, m.rnd, m.vrnd,
        m.acpid,
        (long long)m.ts.tv_sec, m.ts.tv_usec,
        m.value);
}

Message decode_message(Message m) {
    Message res;
    res.mstype = m.mstype;
    res.inst = ntohs(m.inst);
    res.rnd = m.rnd;
    res.vrnd = m.vrnd;
    res.ts = m.ts;
#if __BYTE_ORDER == __BIG_ENDIAN
    res.acpid = m.acpid;
#else
    res.acpid = invert_byte_order(m.acpid),
#endif
    strcpy(res.value ,m.value);
    return res;
}