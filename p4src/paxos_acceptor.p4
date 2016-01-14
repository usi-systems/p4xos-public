#include "includes/paxos_headers.p4"
#include "includes/paxos_parser.p4"
#include "l2_control.p4"

#define INSTANCE_COUNT 65536 // infer from INSTANCE_SIZE: 2^16 instances

#define PAXOS_1A 1 
#define PAXOS_1B 2 
#define PAXOS_2A 3 
#define PAXOS_2B 4 

header_type ingress_metadata_t {
    fields {
        round : ROUND_SIZE;
    }
}

metadata ingress_metadata_t paxos_packet_metadata;

register datapath_id {
    width: 64;
    static : acceptor_tbl;
    instance_count : 1; 
}

register rounds_register {
    width : ROUND_SIZE;
    instance_count : INSTANCE_COUNT;
}

register vrounds_register {
    width : ROUND_SIZE;
    instance_count : INSTANCE_COUNT;
}

register values_register {
    width : VALUE_SIZE;
    instance_count : INSTANCE_COUNT;
}

// Copying Paxos-fields from the register to meta-data structure. The index
// (i.e., paxos instance number) is read from the current packet. Could be
// problematic if the instance exceeds the bounds of the register.
action read_round() {
    register_read(paxos_packet_metadata.round, rounds_register, paxos.instance); 
}

// Receive Paxos 1A message, send Paxos 1B message
action handle_1a() {
    modify_field(paxos.msgtype, PAXOS_1B);                            // Create a 1B message
    register_read(paxos.vround, vrounds_register, paxos.instance);    // paxos.vround = vrounds_register[paxos.instance]
    register_read(paxos.value, values_register, paxos.instance);      // paxos.value  = values_register[paxos.instance]
    register_read(paxos.acceptor, datapath_id, 0);                    // paxos.acceptor = datapath_id
    register_write(rounds_register, paxos.instance, paxos.round);     // rounds_register[paxos.instance] = paxos.round
}

// Receive Paxos 2A message, send Paxos 2B message
action handle_2a() {
    modify_field(paxos.msgtype, PAXOS_2B);				              // Create a 2B message
    register_write(rounds_register, paxos.instance, paxos.round);     // rounds_register[paxos.instance] = paxos.round
    register_write(vrounds_register, paxos.instance, paxos.round);    // vrounds_register[paxos.instance] = paxos.round
    register_write(values_register, paxos.instance, paxos.value);     // values_register[paxos.instance] = paxos.value
    register_read(paxos.acceptor, datapath_id, 0);                    // paxos.acceptor = datapath_id
}

table round_tbl {
    actions { read_round; }
}

table acceptor_tbl {
    reads   { paxos.msgtype : exact; }
    actions { handle_1a; handle_2a; _drop; }
}

control ingress {
    apply(smac);                 /* l2 learning switch logic */
    apply(dmac);
    if (valid(paxos)) {          /* check if we have a paxos packet */
        apply(round_tbl);
        if (paxos_packet_metadata.round <= paxos.round) { /* if the round number is greater than one you've seen before */
            apply(acceptor_tbl);
         } else apply(drop_tbl); /* deprecated prepare/promise */
     }
}
