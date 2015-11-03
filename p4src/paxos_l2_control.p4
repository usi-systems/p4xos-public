
#include "paxos_acceptor.p4"

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
    generate_digest(MAC_LEARN_RECEIVER, mac_learn_digest);
}

table smac {
    reads {
        ethernet.srcAddr : exact;
    }
    actions {mac_learn; _nop;}
    size : 512;
}

action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}

action broadcast() {
    modify_field(intrinsic_metadata.mcast_grp, 1);
}

table dmac {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
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
    apply(smac);                 /* l2 learning switch logic */
    apply(dmac);
                                 /* TODO(check if we can split out control */
    if (valid(paxos)) {          /* check if we have a paxos packet */
        apply(round_tbl);
        if (paxos_packet_metadata.round <= paxos.round) { /* if the round number is greater than one you've seen before */
            apply(acceptor_tbl);
         } else apply(drop_tbl); /* deprecated prepare/promise */
     }
}

control egress {
    if(standard_metadata.ingress_port == standard_metadata.egress_port) {
        apply(mcast_src_pruning);
    }

}
