#include "includes/headers.p4"
#include "includes/parser.p4"


header_type intrinsic_metadata_t {
    fields {
        mcast_grp : 4;
        egress_rid : 4;
        mcast_hash : 16;
        lf_field_list: 32;
    }
}

metadata intrinsic_metadata_t intrinsic_metadata;

action _drop() {
    drop();
}

action _nop() {

}

#define MAC_LEARN_RECEIVER 1024

field_list mac_learn_digest {
    ethernet.srcAddr;
    standard_metadata.ingress_port;
}

action mac_learn() {
    clone_ingress_pkt_to_egress(MAC_LEARN_RECEIVER, mac_learn_digest);
    modify_field(intrinsic_metadata.mcast_grp, 1);
}

action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}

action broadcast() {
    modify_field(intrinsic_metadata.mcast_grp, 1);
}

table mac_table {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
        mac_learn;
        forward;
        broadcast;
    }
    size : 512;
}

table mcast_src_pruning {
    reads {
        standard_metadata.instance_type : exact;
    }
    actions {_nop; _drop;}
    size : 1;
}

table drop_tbl {
    actions { _drop; }
    size : 1;
}

control ingress {
    apply(mac_table);                /* l2 learning switch logic */
}
control egress {
    if(standard_metadata.ingress_port == standard_metadata.egress_port) {
        apply(mcast_src_pruning);
    }
}
