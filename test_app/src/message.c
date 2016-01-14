#include <stdio.h>
#include <strings.h>
#include <inttypes.h>
#include <endian.h>
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
        "value:    %s\n",
        m.mstype, ntohs(m.inst), m.rnd, m.vrnd,
#if __BYTE_ORDER == __BIG_ENDIAN
        m.acpid,
#else
        invert_byte_order(m.acpid),
#endif
        m.value);
}