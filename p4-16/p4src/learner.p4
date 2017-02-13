#include <core.p4>
#include <v1model.p4>
#include "includes/header.p4"
#include "includes/parser.p4"

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    register<bit<ROUND_SIZE>>(INSTANCE_COUNT) registerRound;
    register<bit<VALUE_SIZE>>(INSTANCE_COUNT) registerValue;
    register<bit<8>>(INSTANCE_COUNT) registerHistory2B;

    action read_round() {
        registerRound.read(meta.paxos_metadata.round, hdr.paxos.inst);
        meta.paxos_metadata.set_drop = 1;
        registerHistory2B.read(meta.paxos_metadata.ack_acceptors, hdr.paxos.inst);
    }

    table round_tbl {
        key = {}
        actions = {
            read_round;
        }
        size = 8;
        default_action = read_round;
    }

    action handle_2b() {
        registerRound.write(hdr.paxos.inst, hdr.paxos.rnd);
        registerValue.write(hdr.paxos.inst, hdr.paxos.paxosval);
        // Limit to 8 bits for left shift operation
        bit<8> acptid = 8w1 << (bit<8>)hdr.paxos.acptid;
        meta.paxos_metadata.ack_acceptors = meta.paxos_metadata.ack_acceptors | acptid;
        registerHistory2B.write(hdr.paxos.inst, meta.paxos_metadata.ack_acceptors);
    }

    table learner_tbl {
        key = {hdr.paxos.msgtype : exact;}
        actions = {
            handle_2b;
        }
        size = 1;
        default_action = handle_2b;
    }

    action handle_new_value() {
        registerRound.write(hdr.paxos.inst, hdr.paxos.rnd);
        registerValue.write(hdr.paxos.inst, hdr.paxos.paxosval);
        bit<8> acptid = 8w1 << (bit<8>)hdr.paxos.acptid;
        registerHistory2B.write(hdr.paxos.inst, acptid);
    }

    table reset_consensus_instance {
        key = {hdr.paxos.msgtype : exact;}
        actions = {
            handle_new_value;
        }
        size = 1;
        default_action = handle_new_value;
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
                round_tbl.apply();
                if (hdr.paxos.rnd > meta.paxos_metadata.round) {
                    reset_consensus_instance.apply();
                }
                else if (hdr.paxos.rnd == meta.paxos_metadata.round) {
                    learner_tbl.apply();
                }
                // TODO: replace this with counting number of 1 in Binary
                // e.g count_number_of_1_binary(paxos_metadata.acceptors) == MAJORITY
                if (meta.paxos_metadata.ack_acceptors == 6       // 0b110
                    || meta.paxos_metadata.ack_acceptors == 5    // 0b101
                    || meta.paxos_metadata.ack_acceptors == 3)   // 0b011
                {
                    // deliver the value
                    forward_tbl.apply();
                }
            }
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
        default_action =  set_UDPdstPort(APPLICATION_PORT);
    }

    apply {
        transport_tbl.apply();
    }
}
V1Switch(TopParser(), verifyChecksum(), ingress(), egress(), computeChecksum(), TopDeparser()) main;