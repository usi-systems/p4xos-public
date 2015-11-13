#include "includes/headers.p4"
#include "includes/parser.p4"
#include "includes/paxos_headers.p4"
#include "includes/paxos_parser.p4"


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


register instance_register {
    width : INSTANCE_SIZE;
    instance_count : 1;
}

//  This function read num_inst stored in the register and copy it to
//  the current packet. Then it increased the num_inst by 1, and write
//  it back to the register
action handle_2a() {
    register_read(paxos.instance, instance_register, 0);
    add_to_field(paxos.instance, 1);
    register_write(instance_register, 0, paxos.instance);
}

table sequence_tbl {
    reads   { paxos.msgtype : exact; }
    actions { handle_2a; _nop; }
    size : 1;
}

control ingress {
    apply(smac);                 /* l2 learning switch logic */
    apply(dmac);
                                 
    if (valid(paxos)) {          /* check if we have a paxos packet */
        apply(sequence_tbl);     /* increase paxos instance number */
     }
}

control egress {
    if(standard_metadata.ingress_port == standard_metadata.egress_port) {
        apply(mcast_src_pruning);
    }

}
