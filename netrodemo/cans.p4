header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

header_type ipv4_t {
    fields {
        version : 4;
        ihl : 4;
        diffserv : 8;
        totalLen : 16;
        identification : 16;
        flags : 3;
        fragOffset : 13;
        ttl : 8;
        protocol : 8;
        hdrChecksum : 16;
        src : 32;
        dst: 32;
    }
}

header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        length_ : 16;
        checksum : 16;
    }
}

// Headers for Paxos

header_type paxos_t {
    fields {
        msgtype : 32;
        inst    : 32;
        rnd     : 16;
        vrnd    : 16;
        acpt    : 32;
        valsize : 32;  // <---- content length is set here
    }
}

/*
 * 1. Could we set content field to a variable length field?
 * The content length is the value of valsize field of Paxos header.
 * 2. Does SDK allow the width of content greater than 32?
 */
header_type value_t {
    fields {
        content : 32;
    }
}

header ethernet_t ethernet;
header ipv4_t ipv4;
header udp_t udp;
header paxos_t paxos;
header value_t value;


#define ETHERTYPE_IPV4 0x0800
#define UDP_PROTOCOL 0x11
#define PAXOS_PROTOCOL 0x8888

parser start {
    return parse_ethernet;
}

parser parse_ethernet {
    extract(ethernet);
    return select(latest.etherType) {
        ETHERTYPE_IPV4 : parse_ipv4; 
        default : ingress;
    }
}

parser parse_ipv4 {
    extract(ipv4);
    return select(latest.protocol) {
        UDP_PROTOCOL : parse_udp;
        default : ingress;
    }
}

parser parse_udp {
    extract(udp);
    return select(udp.dstPort) {
        PAXOS_PROTOCOL: parse_paxos;
        default: ingress;
    }
}

parser parse_paxos {
    extract(paxos);
    return select(paxos.valsize) {
        0 : ingress;
        default : parse_value;
    }
}

parser parse_value {
    extract(value);
    return ingress;
}

primitive_action paxos_func();
primitive_action seq_func();


action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}

table mac_tbl {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
        forward;
    }
    size : 8;
}

action seq_handle() {
    seq_func();
}

action paxos_handle() {
    paxos_func();
}

table paxos_tbl {
    reads {
        paxos.msgtype : exact;
    }
    actions {
        paxos_handle;
        seq_handle;
    }
    size : 8;
}

control ingress {
    apply(mac_tbl);

    /*
     *  ISSUE: "valid" operation is not supported
     * if valid(paxos) {
     *  apply(paxos_tbl);
     * }
     */

    apply(paxos_tbl);
}

