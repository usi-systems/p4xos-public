action handle_icmp_request() {
    // Swap Ethernet address
    hdr.ethernet.dstAddr = hdr.ethernet.srcAddr;
    my_mac_address.read(hdr.ethernet.srcAddr, 0);
    // Swap IP address
    bit<32> ipdst = hdr.ipv4.dstAddr;
    hdr.ipv4.dstAddr = hdr.ipv4.srcAddr;
    hdr.ipv4.srcAddr = ipdst;
    // ICMP Reply
    hdr.icmp.icmpType = 0;
    hdr.icmp.hdrChecksum = hdr.icmp.hdrChecksum + (16w8<<8);
}

action handle_icmp_reply() {

}

table icmp_tbl() {
    key = {hdr.icmp.icmpType : exact;}
    actions = {
        handle_icmp_request;
        handle_icmp_reply;
        _drop;
    }
    size = 4;
    default_action  = _drop();
}