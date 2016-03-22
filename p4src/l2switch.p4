


header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

header ethernet_t ethernet;

parser start {
    return parse_ethernet;
}

parser parse_ethernet {
    extract(ethernet);
    return ingress;
}


action forward(port) {
    modify_field(standard_metadata.egress_spec, port);
}


table fwd_tbl {
    reads {
        standard_metadata.ingress_port : exact;
    }
    actions {
        forward;
    }
    size : 16;
}

control ingress {
    apply(fwd_tbl);
}
