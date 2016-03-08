#define PAXOS_ENABLE 1
#include "includes/headers.p4"
#include "includes/parser.p4"
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

action _drop() {
    drop();
}

action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}

table drop_tbl {
    actions { _drop; }
    size : 1;
}

table dmac_tbl {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
        forward;
    }
    size : 16;
}


control ingress {
    if (valid(ipv4)) {
        apply(dmac_tbl);

        if (valid(paxos)) {          /* check if we have a paxos packet */
            apply(role_tbl);
            if (switch_metadata.role == IS_COORDINATOR) {
                apply(sequence_tbl);     /* increase paxos instance number */
            }
            else {
                if (switch_metadata.role == IS_ACCEPTOR) {
                    apply(round_tbl);
                    if (paxos_packet_metadata.round <= paxos.rnd) { /* if the round number is greater than one you've seen before */
                        apply(acceptor_tbl);
                     }
                     /* else apply(drop_tbl); /* deprecated prepare/promise */
                }
            }
         }
    } else if (valid(ipv6)) {
        apply(drop_tbl);
    }
}