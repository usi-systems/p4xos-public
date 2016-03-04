#include "includes/headers.p4"
#include "includes/parser.p4"



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
    if (valid(ipv6)) {
        apply(drop_tbl);
    } else {
        apply(dmac_tbl);
    }
}
