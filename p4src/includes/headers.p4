// Template headers.p4 file for minion
// Edit this file as needed for your P4 program

// Here's an ethernet header to get started.

#include "routing_header.p4"

header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

header ethernet_t ethernet;

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
        srcAddr : 32;
        dstAddr: 32;
    }
}

header ipv4_t ipv4;


header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        length_ : 16;
        checksum : 16;
    }
}

header udp_t udp;

field_list udp_checksum_list {
    ipv4.srcAddr;
    ipv4.dstAddr;
    8'0;
    ipv4.protocol;
    routing_metadata.udpLength;
    udp.srcPort;
    udp.dstPort;
    udp.length_;
    payload;
}

field_list_calculation udp_checksum {
    input {
        udp_checksum_list;
    }
    algorithm : csum16;
    output_width : 16;
}

calculated_field udp.checksum {
    verify udp_checksum if (valid(udp));
    update udp_checksum if (valid(udp));
}

header_type paxos_t {
    fields {
        pxtype : 8;
        instance : 16;
        round : 8;
        vround : 8;
        value : 32;
    }
}

header paxos_t paxos;
