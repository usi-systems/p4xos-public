//uncomment to enable openflow
//#define OPENFLOW_ENABLE

#ifdef OPENFLOW_ENABLE
    #include "openflow.p4"
#endif /* OPENFLOW_ENABLE */
#include "minion.p4"
#include "includes/routing_header.p4"

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
    /*
    ipv4.totalLen is the byte size of IP packet (IP header + UDP header + UDP payload)
    To obtain the total length of UDP packet we need to subtract the IP header length, which is ipv4.ihl * 4 [bytes]
    */
    modify_field(routing_metadata.ipv4Length, ipv4.ihl);
    add_to_field(routing_metadata.ipv4Length, ipv4.ihl);
    add_to_field(routing_metadata.ipv4Length, ipv4.ihl);
    add_to_field(routing_metadata.ipv4Length, ipv4.ihl);
    modify_field(routing_metadata.udpLength, ipv4.totalLen);
    subtract_from_field(routing_metadata.udpLength, routing_metadata.ipv4Length);

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
#ifdef OPENFLOW_ENABLE
        openflow_apply;
        openflow_miss;
#endif /* OPENFLOW_ENABLE */
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
#ifdef OPENFLOW_ENABLE
    apply(packet_out) {
        nop {
#endif /* OPENFLOW_ENABLE */
            apply(smac);
            apply(dmac);
            if (valid(paxos)) {
                apply(round_tbl);
                if (swmem.round <= paxos.round) {
                    apply(accept_tbl);
                } else apply(drop_tbl);
            }
#ifdef OPENFLOW_ENABLE
        }
    }

    process_ofpat_ingress ();
#endif /* OPENFLOW_ENABLE */
}

control egress {
    if(standard_metadata.ingress_port == standard_metadata.egress_port) {
        apply(mcast_src_pruning);
    }

#ifdef OPENFLOW_ENABLE
    process_ofpat_egress();
#endif /*OPENFLOW_ENABLE */
}
