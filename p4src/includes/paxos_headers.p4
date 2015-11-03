// Headers for Paxos

header_type paxos_t {
    fields {
        msgtype : 8;         // indicates the message type e.g., 1A, 1B, etc.
        instance : 16;    // instance number
        round : 8;        // round number
        vround : 8;       // round in which an acceptor casted a vote
        value : 24;       // the value the acceptor voted for
    }
}

header paxos_t paxos;
