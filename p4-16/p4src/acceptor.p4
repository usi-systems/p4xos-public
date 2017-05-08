#include <core.p4>
#include <v1model.p4>
#include "includes/header.p4"
#include "includes/parser.p4"

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    register<bit<DATAPATH_SIZE>>(1) registerAcceptorID;
    register<bit<ROUND_SIZE>>(INSTANCE_COUNT) registerRound;
    register<bit<ROUND_SIZE>>(INSTANCE_COUNT) registerVRound;
    register<bit<VALUE_SIZE>>(INSTANCE_COUNT) registerValue;

    register<bit<48>>(1) learner_mac_address;
    register<bit<32>>(1) learner_address;

    action _drop() {
        mark_to_drop();
    }

    action read_round() {
        registerRound.read(meta.paxos_metadata.round, hdr.paxos.inst);
    }

    action handle_1a() {
        hdr.paxos.msgtype = PAXOS_1B;
        registerVRound.read(hdr.paxos.vrnd, hdr.paxos.inst);
        registerValue.read(hdr.paxos.paxosval, hdr.paxos.inst);
        registerAcceptorID.read(hdr.paxos.acptid, 0);
        registerRound.write(hdr.paxos.inst, hdr.paxos.rnd);
    }

    action handle_2a() {
        hdr.paxos.msgtype = PAXOS_2B;
        registerAcceptorID.read(hdr.paxos.acptid, 0);
        registerRound.write(hdr.paxos.inst, hdr.paxos.rnd);
        registerVRound.write(hdr.paxos.inst, hdr.paxos.rnd);
        registerValue.write(hdr.paxos.inst, hdr.paxos.paxosval);
    }

    table acceptor_tbl {
        key = {hdr.paxos.msgtype : exact;}
        actions = {
            handle_1a;
            handle_2a;
            _drop;
        }
        size = 4;
        default_action = _drop();
    }

    action forward(PortId port, bit<48> mac_dst, bit<32> ip_dst, bit<16> udp_dst) {
        standard_metadata.egress_spec = port;
        hdr.ethernet.dstAddr = mac_dst;
        // Set MAC destinate to learners
        hdr.ethernet.dstAddr = mac_dst;
        // Set IP destinate to learners
        hdr.ipv4.dstAddr = ip_dst;
        // Set UDP destinate to learners
        hdr.udp.dstPort = udp_dst;
    }

    table transport_tbl {
        key = { hdr.ipv4.dstAddr : exact; }
        actions = {
            _drop;
             forward;
        }
        size = 2;
        default_action =  _drop();
    }

    #include "arp.p4"
    #include "icmp.p4"

    apply {
        if (hdr.arp.isValid()) {
            arp_tbl.apply();
        }
        else if (hdr.ipv4.isValid()) {
            if (hdr.paxos.isValid()) {
                read_round();
                if (hdr.paxos.rnd >= meta.paxos_metadata.round) {
                    acceptor_tbl.apply();
                    transport_tbl.apply();
                }
            } else if (hdr.icmp.isValid()) {
                icmp_tbl.apply();
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