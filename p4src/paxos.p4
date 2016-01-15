#include "l2switch.p4"
#include "coordinator.p4"
#include "acceptor.p4"

#define IS_COORDINATOR 1
#define IS_ACCEPTOR 2


header_type switch_metadata_t {
    fields {
        role : 8;
    }
}

metadata switch_metadata_t switch_metadata;

register role_register {
    width : 8;
    instance_count : 1;
}

action read_role() {
    register_read(switch_metadata.role, role_register, 0);
}

table role_tbl {
    actions { read_role; }
}

control paxos_ingress {
    ingress();
    if (valid(paxos)) {          /* check if we have a paxos packet */
        apply(role_tbl);
        if (switch_metadata.role == IS_COORDINATOR) {
            apply(sequence_tbl);     /* increase paxos instance number */
        }
        else {
            apply(round_tbl);
            if (paxos_packet_metadata.round <= paxos.round) { /* if the round number is greater than one you've seen before */
                apply(acceptor_tbl);
             } else apply(drop_tbl); /* deprecated prepare/promise */
        }
     }
}