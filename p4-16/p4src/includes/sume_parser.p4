#ifndef _PARSER_P4_
#define _PARSER_P4_


#define ETHERTYPE_IPV4 16w0x0800
#define UDP_PROTOCOL 8w0x11
#define PAXOS_PROTOCOL 16w0x8888

parser TopParser(packet_in b,
                 out headers p,
                 out paxos_metadata_t user_metadata,
                 inout sume_metadata_t sume_metadata) {

    state start {
        transition parse_ethernet;
    }

    state parse_ethernet {
        b.extract(p.ethernet);
        transition select(p.ethernet.etherType) {
            ETHERTYPE_IPV4 : parse_ipv4;
        }
    }

    state parse_ipv4 {
        b.extract(p.ipv4);
        transition select(p.ipv4.protocol) {
            UDP_PROTOCOL : parse_udp;
            default : accept;
        }
    }

    state parse_udp {
        b.extract(p.udp);
        transition select(p.udp.dstPort) {
            PAXOS_PROTOCOL : parse_paxos;
            default : accept;
        }
    }

    state parse_paxos {
        b.extract(p.paxos);
        transition accept;
    }
}

control TopDeparser(packet_out packet,
                    in headers hdr,
                    in paxos_metadata_t user_metadata,
                    inout sume_metadata_t sume_metadata) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.udp);
        packet.emit(hdr.paxos);
    }
}


#endif