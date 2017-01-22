#ifndef _PAXOS_HEADERS_P4_
#define _PAXOS_HEADERS_P4_

// Headers for Paxos
#define PAXOS_1A 0
#define PAXOS_1B 1
#define PAXOS_2A 2
#define PAXOS_2B 3

#define MSGTYPE_SIZE 16
#define INSTANCE_SIZE 32
#define ROUND_SIZE 16
#define DATAPATH_SIZE 16
#define VALUE_SIZE 256

#define INSTANCE_COUNT 65536

header_type paxos_t {
    fields {
        msgtype  : MSGTYPE_SIZE;     // indicates the message type e.g., 1A, 1B, etc.
        inst     : INSTANCE_SIZE;    // instance number
        rnd      : ROUND_SIZE;       // round number
        vrnd     : ROUND_SIZE;       // round in which an acceptor casted a vote
        acptid   : DATAPATH_SIZE;    // Switch ID
        paxoslen : 32;               // the length of paxos_value
        paxosval : VALUE_SIZE;       // the value the acceptor voted for
    }
}

header paxos_t paxos;

#endif
