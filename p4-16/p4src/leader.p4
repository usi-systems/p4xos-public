#include <core.p4>
#include "SimpleSumeSwitch.p4"
#include "includes/header.p4"
#include "includes/sume_parser.p4"

#define REG_READ 8w0
#define REG_WRITE 8w0

extern void mark_to_drop();
extern void ctrlInstane_reg_rw(in bit<1> index, in bit<32> newVal, in bit<8> opCode, out bit<32> result);

control TopPipe(inout headers hdr,
                inout paxos_metadata_t user_metadata,
                inout sume_metadata_t sume_metadata) {

    action _drop() {
        mark_to_drop();
    }

    action increase_instance() {
        bit<1> index = 1w0;
        bit<32> current_instance = 32w0;
        bit<32> dont_care = 1w0;
        ctrlInstane_reg_rw(index, dont_care, REG_READ, current_instance);
        hdr.paxos.inst = current_instance;
        current_instance = current_instance + 1;
        ctrlInstane_reg_rw(index, current_instance, REG_WRITE, dont_care);
        user_metadata.set_drop = 0;
    }

    action reset_instance() {
        bit<1> index = 1w0;
        bit<32> reset_value = 32w0;
        bit<32> dont_care = 1w0;
        ctrlInstane_reg_rw(index, reset_value, REG_WRITE, dont_care);

        // Do not need to forward this message
        user_metadata.set_drop = 1;
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
        sume_metadata.dst_port = port;
        hdr.udp.dstPort = acceptorPort;
    }

    table transport_tbl {
        key = { user_metadata.set_drop : exact; }
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

SimpleSumeSwitch(TopParser(), TopPipe(), TopDeparser()) main;