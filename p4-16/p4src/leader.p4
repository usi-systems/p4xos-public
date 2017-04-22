#include <core.p4>
#include <v1model.p4>
#include "includes/header.p4"
#include "includes/parser.p4"

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    register<bit<INSTANCE_SIZE>>(1) registerInstance;

    action _drop() {
        mark_to_drop();
    }

    action increase_instance() {
        registerInstance.read(hdr.paxos.inst, 0);
        hdr.paxos.inst = hdr.paxos.inst + 1;
        registerInstance.write(0, hdr.paxos.inst);
        meta.paxos_metadata.set_drop = 0;

    }

    action reset_instance() {
        registerInstance.write(0, 0);
        // Do not need to forward this message
        meta.paxos_metadata.set_drop = 1;
    }

    table leader_tbl {
        key = {hdr.paxos.msgtype : exact;}
        actions = {
            increase_instance;
            reset_instance;
            _drop;
        }
        size = 4;
        default_action = _drop();
    }


    action forward(PortId port, bit<16> acceptorPort) {
        standard_metadata.egress_spec = port;
        hdr.udp.dstPort = acceptorPort;
    }

    table transport_tbl {
        key = { meta.paxos_metadata.set_drop : exact; }
        actions = {
            _drop;
             forward;
        }
        size = 2;
        default_action =  _drop();
    }

    apply {
        if (hdr.ipv4.isValid()) {
            if (hdr.paxos.isValid()) {
                leader_tbl.apply();
                transport_tbl.apply();
            }
        }
    }
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    table place_holder_table {
        actions = {
            NoAction;
        }
        size = 2;
        default_action = NoAction();
    }
    apply {
        place_holder_table.apply();
    }
}

V1Switch(TopParser(), verifyChecksum(), ingress(), egress(), computeChecksum(), TopDeparser()) main;