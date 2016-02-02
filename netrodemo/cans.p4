header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

header_type arp_t {
    fields {
        hrd : 16;
        pro : 16;
        hln : 8;
        pln : 8;
        op  : 16;
        sha : 48;
        spa : 32;
        tha : 48;
        tpa : 32;
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
        instance : 32;
        msgtype : 16;
        round : 16; 
        vround : 16; 
        value : 16;
        acceptor: 64;
    }
}

header ethernet_t ethernet;
header arp_t arp;
header ipv4_t ipv4;
header udp_t udp;
header paxos_t paxos;


#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IPV4 0x0800
#define UDP_PROTOCOL 0x11
#define PAXOS_PROTOCOL 0x8888

parser start {
    return parse_ethernet;
}

parser parse_ethernet {
    extract(ethernet);
    return select(latest.etherType) {
        ETHERTYPE_ARP : parse_arp;
        ETHERTYPE_IPV4 : parse_ipv4; 
        default : ingress;
    }
}

parser parse_arp {
    extract(arp);
    return ingress;
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
    return ingress;
}

primitive_action paxos_func();


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

action paxos_handle() {
    paxos_func();
}

table paxos_tbl {
    actions {
        paxos_handle;
    }
    size : 8;
}


control ingress {
    apply(mac_tbl);
    apply(paxos_tbl);
}

