#include "includes/headers.p4"
#include "includes/parser.p4"

#define NUM_INST 16
#define ROUND_SZ 8
#define VALUE_SZ 32


header_type ingress_metadata_t {
    fields {
        round : ROUND_SZ;
        vround : ROUND_SZ;
        value : VALUE_SZ;
    }
}

metadata ingress_metadata_t swmem;

register rounds {
    width : ROUND_SZ;
    instance_count : NUM_INST;
}

register vrounds {
    width : ROUND_SZ;
    instance_count : NUM_INST;
}

register values {
    width : VALUE_SZ;
    instance_count : NUM_INST;
}


action read_round() {
    register_read(swmem.round, rounds, paxos.instance); 
}

action promise() {
    register_read(swmem.vround, vrounds, paxos.instance); 
    register_read(swmem.value, values, paxos.instance); 
    register_write(rounds, paxos.instance, paxos.round);
    modify_field(paxos.vround, swmem.vround);
    modify_field(paxos.value, swmem.value);
    add_to_field(paxos.pxtype, 1);

}

action accept() {
    add_to_field(paxos.pxtype, 1);
    register_write(rounds, paxos.instance, paxos.round);
    register_write(vrounds, paxos.instance, paxos.round);
    register_write(values, paxos.instance, paxos.value);
}

table round_tbl { actions { read_round; } }

table accept_tbl {
    reads {
        paxos.pxtype : exact;
    }
    actions { promise; accept; _drop; }
}
