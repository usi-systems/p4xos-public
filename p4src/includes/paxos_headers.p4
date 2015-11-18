// Headers for Paxos

#define MSGTYPE_SIZE 8
#define ROUND_SIZE 8
#define INSTANCE_SIZE 16
#define VALUE_SIZE 512
#define DATAPATH_SIZE 64

header_type paxos_t {
    fields {
        msgtype : MSGTYPE_SIZE;         // indicates the message type e.g., 1A, 1B, etc.
        instance : INSTANCE_SIZE;    // instance number
        round : ROUND_SIZE;        // round number
        vround : ROUND_SIZE;       // round in which an acceptor casted a vote
        acceptor: DATAPATH_SIZE;
        value : VALUE_SIZE;       // the value the acceptor voted for
    }
}

header paxos_t paxos;
