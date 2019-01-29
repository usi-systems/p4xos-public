#include "includes/headers.p4"
#include "includes/parser.p4"
#include "includes/paxos_headers.p4"
#include "includes/paxos_parser.p4"

#define INSTANCE_COUNT 65536

header_type ingress_metadata_t {
    fields {
        round : ROUND_SIZE;
        set_drop : 1;
        count : 8;
        acceptors : 8;
    }
}

metadata ingress_metadata_t paxos_packet_metadata;

register rounds_register {
    width : ROUND_SIZE;
    instance_count : INSTANCE_COUNT;
}

register values_register {
    width : VALUE_SIZE;
    instance_count : INSTANCE_COUNT;
}


// Count the number of acceptors has cast a vote
register history2B {
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
    register_read(paxos_packet_metadata.round, rounds_register, paxos.instance);
    modify_field(paxos_packet_metadata.set_drop, 1);
    register_read(paxos_packet_metadata.acceptors, history2B, paxos.instance);
}

table tbl_rnd {
    actions { read_round; }
    size : 1;
}

// Receive Paxos 2A message, send Paxos 2B message
action handle_2b() {
    // TODO: the 2 lines below only needed for the first time
    register_write(rounds_register, paxos.instance, paxos.round);
    register_write(values_register, paxos.instance, paxos.value);
    // Acknowledge accepted vote
    modify_field(paxos_packet_metadata.acceptors, paxos_packet_metadata.acceptors | (1 << paxos.acceptor));
    register_write(history2B, paxos.instance, paxos_packet_metadata.acceptors);
}

action handle_new_value() {
    register_write(rounds_register, paxos.instance, paxos.round);
    register_write(values_register, paxos.instance, paxos.value);
    register_write(history2B, paxos.instance, 1 << paxos.acceptor);
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
    modify_field(paxos_packet_metadata.set_drop, 0);
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
        paxos_packet_metadata.set_drop : exact;
    }
    actions { _drop; _nop; }
    size : 2;
}

control ingress {
    if (valid(paxos)) {
        apply(tbl_rnd);
        if (paxos.round > paxos_packet_metadata.round) {
            apply(reset_tbl);
        }
        else if (paxos.round == paxos_packet_metadata.round) {
            apply(learner_tbl);
        }
        // TODO: replace this with counting number of 1 in Binary
        // e.g count_number_of_1_binary(paxos_packet_metadata.acceptors) == MAJORITY
        if (paxos_packet_metadata.acceptors == 6       // 0b110
            or paxos_packet_metadata.acceptors == 5    // 0b101
            or paxos_packet_metadata.acceptors == 3)   // 0b011
        {
            // deliver the value
            apply(forward_tbl);
        }
    }
}

control egress {
    apply(drop_tbl);
}
