#include <core.p4>
#include <v1model.p4>
#include "includes/header.p4"
#include "includes/parser.p4"

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    register<bit<INSTANCE_SIZE>>(1) registerInstance;

    action increase_instance() {
        registerInstance.read(hdr.paxos.inst, 0);
        hdr.paxos.inst = hdr.paxos.inst + 1;
        registerInstance.write(0, hdr.paxos.inst);
    }

    action reset_instance() {
        registerInstance.write(0, 0);
    }

    table leader_tbl {
        key = {hdr.paxos.msgtype : exact;}
        actions = {
            increase_instance;
            reset_instance;
            NoAction;
        }
        size = 4;
        default_action = NoAction();
    }
    action forward(PortId port) {
        standard_metadata.egress_spec = port;
        meta.paxos_metadata.set_drop = 0;
    }

    table forward_tbl {
        key = {}
        actions = {
            forward;
        }
        size = 1;
        default_action = forward(DROP_PORT);
    }

    apply {
        if (hdr.ipv4.isValid()) {
            if (hdr.paxos.isValid()) {
                leader_tbl.apply();
            }
            forward_tbl.apply();
        }
    }
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {

    action _drop() {
        mark_to_drop();
    }

    action set_UDPdstPort(bit<16> dstPort) {
        hdr.udp.dstPort = dstPort;
    }

    table transport_tbl {
        key = { meta.paxos_metadata.set_drop : exact; }
        actions = {
            _drop;
             set_UDPdstPort;
        }
        size = 2;
        default_action =  set_UDPdstPort(ACCEPTOR_PORT);
    }

    apply {
        transport_tbl.apply();
    }
}


V1Switch(TopParser(), verifyChecksum(), ingress(), egress(), computeChecksum(), TopDeparser()) main;