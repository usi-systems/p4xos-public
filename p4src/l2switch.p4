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
}

action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}

action broadcast() {
    modify_field(intrinsic_metadata.mcast_grp, 1);
}

action encap_cpu_header() {
    remove_header(arp);
    add_header(ipv4);
    modify_field(ipv4.protocol, P4_PROTOCOL);
    modify_field(ipv4.totalLen, 21);
    add_header(cpu_header);
    modify_field(cpu_header.in_port, standard_metadata.ingress_port);
}

table smac_table {
    reads {
        ethernet.srcAddr : exact;
    }
    actions {
        mac_learn;
        _nop;
    }
    size : 512;
}

table dmac_table {
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

table encap_tbl {
    actions {encap_cpu_header; }
    size : 1;
}
control ingress {
    apply(smac_table);
    apply(dmac_table);
}

control egress {
    if(standard_metadata.instance_type == 1) {
        apply(encap_tbl);
    }

    if(standard_metadata.ingress_port == standard_metadata.egress_port) {
        apply(mcast_src_pruning);
    }
}
