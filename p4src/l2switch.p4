#include "includes/headers.p4"
#include "includes/parser.p4"

action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}

table dmac_table {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
        forward;
    }
    size : 16;
}

control ingress {
    apply(dmac_table);
}
