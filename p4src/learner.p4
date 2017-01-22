#include "includes/headers.p4"
#include "includes/parser.p4"
#include "includes/paxos_headers.p4"
#include "includes/paxos_parser.p4"

header_type ingress_metadata_t {
    fields {
        round : ROUND_SIZE;
        set_drop : 1;
        count : 8;
        acceptors : 8;
        acceptor_id : 8;
    }
}

metadata ingress_metadata_t local_metadata;

register rounds_register {
    width : ROUND_SIZE;
    instance_count : INSTANCE_COUNT;
}

register values_register {
    width : VALUE_SIZE;
    instance_count : INSTANCE_COUNT;
}


// Count the number of acceptors has cast a vote
register count_register {
    width : 8;
    instance_count : INSTANCE_COUNT;
}

action _nop() {

}

action _drop() {
    drop();
}

// Copying Paxos-fields from the register to meta-data structure. The index
// (i.e., paxos instance number) is read from the current packet. Could be
// problematic if the instance exceeds the bounds of the register.
action read_round() {
    register_read(local_metadata.round, rounds_register, paxos.inst);
    modify_field(local_metadata.set_drop, 1);
    register_read(local_metadata.acceptors, count_register, paxos.inst);
    modify_field(local_metadata.acceptor_id, local_metadata.acceptors & (1 << paxos.acptid));
}

table round_tbl {
    actions { read_round; }
    size : 1;
}

// Receive Paxos 2A message, send Paxos 2B message
action handle_2b() {
    // TODO: the 2 lines below only needed for the first time
    register_write(rounds_register, paxos.inst, paxos.rnd);
    register_write(values_register, paxos.inst, paxos.paxosval);
    // Increase the count of accepted votes
    modify_field(local_metadata.acceptors, local_metadata.acceptors | (1 << paxos.acptid));
    register_write(count_register, paxos.inst, local_metadata.acceptors);

}

action handle_new_value() {
    register_write(rounds_register, paxos.inst, paxos.rnd);
    register_write(values_register, paxos.inst, paxos.paxosval);
    register_write(count_register, paxos.inst, 1 << paxos.acptid);
}

table learner_tbl {
    reads   { paxos.msgtype : exact; }
    actions { handle_2b; _drop; }
}

table reset_tbl {
    reads   { paxos.msgtype : exact; }
    actions { handle_new_value; _drop; }
}

action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
    modify_field(local_metadata.set_drop, 0);
}

table forward_tbl {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        forward;
        _drop;
    }
    size : 48;
}

table drop_tbl {
    reads {
        local_metadata.set_drop : exact;
    }
    actions { _drop; _nop; }
    size : 2;
}

control ingress {
    if (valid(ipv4)) {

        if (valid(paxos)) {
            apply(round_tbl);
            if (local_metadata.round == paxos.rnd and local_metadata.acceptor_id == 0) {
                apply(learner_tbl);
            } else if (local_metadata.round < paxos.rnd) {
                apply(reset_tbl);
            }
            if (local_metadata.acceptors == 6       // 0b110
                or local_metadata.acceptors == 5    // 0b101
                or local_metadata.acceptors == 3)   // 0b011
            {
                // deliver the value
                apply(forward_tbl);
            }
        }
    }
}

control egress {
    apply(drop_tbl);
}
