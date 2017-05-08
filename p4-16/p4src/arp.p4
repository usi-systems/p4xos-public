    register<bit<48>>(1) my_mac_address;
    register<bit<32>>(1) my_ip_address;

    action handle_arp_request() {
        // Swap Ethernet address
        hdr.ethernet.dstAddr = hdr.ethernet.srcAddr;
        my_mac_address.read(hdr.ethernet.srcAddr, 0);
        hdr.arp.op = 2;
        hdr.arp.tha = hdr.arp.sha;
        hdr.arp.tpa = hdr.arp.spa;
        // Fill my hardware address
        my_mac_address.read(hdr.arp.sha, 0);
        my_ip_address.read(hdr.arp.spa, 0);
        standard_metadata.egress_spec = standard_metadata.ingress_port;
    }

    action handle_arp_reply() {

    }

    table arp_tbl {
        key = { hdr.arp.op : exact; }
        actions = {
            handle_arp_request;
            handle_arp_reply;
            _drop;
        }
        size = 3;
        default_action = _drop();
    }
