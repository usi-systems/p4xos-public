#ifndef _PAXOS_HEADERS_P4_
#define _PAXOS_HEADERS_P4_

// Headers for Paxos

#define INSTANCE_SIZE 32
#define MSGTYPE_SIZE 16
#define ROUND_SIZE 16
#define DATAPATH_SIZE 32
#define VALUE_SIZE 32

#define INSTANCE_COUNT 65536

header_type paxos_t {
    fields {
        instance : INSTANCE_SIZE; // instance number
        round : ROUND_SIZE;       // round number
        vround : ROUND_SIZE;      // round in which an acceptor casted a vote
        acceptor: DATAPATH_SIZE;  // Switch ID
        msgtype : MSGTYPE_SIZE;   // indicates the message type e.g., 1A, 1B, etc.
        value : VALUE_SIZE;       // the value the acceptor voted for
    }
}

header paxos_t paxos;

#endif