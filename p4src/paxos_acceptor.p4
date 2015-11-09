#include "includes/headers.p4"
#include "includes/parser.p4"
#include "includes/paxos_headers.p4"
#include "includes/paxos_parser.p4"

#define NUMBER_COUNT 65536 // 2^16 instances
#define ROUND_SIZE 8
#define VALUE_SIZE 24

#define PAXOS_1A 1 
#define PAXOS_1B 2 
#define PAXOS_2A 3 
#define PAXOS_2B 4 

header_type ingress_metadata_t {
    fields {
        round : ROUND_SIZE;
        vround : ROUND_SIZE;
        value : VALUE_SIZE;
    }
}

metadata ingress_metadata_t paxos_packet_metadata;

register rounds_register {
    width : ROUND_SIZE;
    instance_count : NUMBER_COUNT;
}

register vrounds_register {
    width : ROUND_SIZE;
    instance_count : NUMBER_COUNT;
}

register values_register {
    width : VALUE_SIZE;
    instance_count : NUMBER_COUNT;
}

// Copying Paxos-fields from the register to meta-data structure. The index
// (i.e., paxos instance number) is read from the current packet. Could be
// problematic if the instance exceeds the bounds of the register.
action read_round() {
    register_read(paxos_packet_metadata.round, rounds_register, paxos.instance); 
}

// Recieve Paxos 1A message, send Paxos 1B message
action handle_1a() {
    modify_field(paxos.msgtype, PAXOS_1B);                                         // Create a 1B message
    register_read(paxos_packet_metadata.vround, vrounds_register, paxos.instance); // paxos_packet_metadata.vround = vrounds_register[paxos.instance]
    register_read(paxos_packet_metadata.value, values_register, paxos.instance);   // paxos_packet_metadata.value  = values_register[paxos.instance]
    register_write(rounds_register, paxos.instance, paxos.round);                  // rounds_register[paxos.instance] = paxos.round
    modify_field(paxos.vround, paxos_packet_metadata.vround);                      // paxos.vround = paxos_packet_metadata.vround
    modify_field(paxos.value, paxos_packet_metadata.value);                        // paxos.value = paxos_packet_metadata.value
}

// Recieve Paxos 2A message, send Paxos 2B message
action handle_2a() {
    modify_field(paxos.msgtype, PAXOS_2B);				          // Create a 2B message
    register_write(rounds_register, paxos.instance, paxos.round);                 // rounds_register[paxos.instance] = paxos.round
    register_write(vrounds_register, paxos.instance, paxos.round);                // vrounds_register[paxos.instance] = paxos.round
    register_write(values_register, paxos.instance, paxos.value);                 // values_register[paxos.instance] = paxos.value
}

table round_tbl {
    actions { read_round; }
}

table acceptor_tbl {
    reads   { paxos.msgtype : exact; }
    actions { handle_1a; handle_2a; _drop; }
}
