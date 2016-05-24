class Packet_input :: Packet(in) {}
class Packet_output :: Packet(out) {}

class Tuple_output :: Tuple(out) {
    struct {
        output : 11 }
    }

class OF_classifier :: System {
    Packet_input instream;
    Packet_output outstream;
    Tuple_output flowstream;
    OF_parser parser;
    ACL lookup;
    method connect = {
        parser.packet_in = instream,
        lookup.request = parser.fields,
        outstream = parser.packet_out,
        flowstream = lookup.response
    }
}

struct of_header_fields {
    port : 3,
    dmac : 48,
    smac : 48,
    type : 16,
    vid : 12,
    pcp : 3,
    sa : 32,
    da : 32,
    proto : 8,
    tos : 6,
    sp : 16,
    dp : 16
}

class ACL :: LookupEngine(TCAM, 1024, 240, 11, 0) {
    class request_tuple :: Tuple(in) {
        struct of_header_fields;
    }
    class response_tuple :: Tuple(out) {
        struct {
            flow : 11
        }
    }
    request_tuple request;
    response_tuple response;

    method send_request = {
        key = request
    }
    method receive_response = {
        response = value
    }
}

class OF_parser :: ParsingEngine (9416*8, 4, ETH_header) {
    class Fields :: Tuple(out) {
        struct {
            port : 3,
            dmac : 48,
            smac : 48,
            type : 16,
            vid : 12,
            pcp : 3,
            sa : 32,
            da : 32,
            proto : 8,
            tos : 6,
            sp : 16,
            dp : 16
        }
    }

    Fields fields;
    const VLAN_TYPE = 0x8100;
    const IP_TYPE = 0x0800;
    // Section sub-class for an Ethernet header
    class ETH_header :: Section(1) {
        struct {
            dmac : 48,
            smac : 48,
            type : 16
        }
        method update = {
            fields.dmac = dmac,
            fields.smac = smac,
            fields.type = type
        }

        method move_to_section =
            if (type == VLAN_TYPE)
                VLAN_header
            else if (type == IP_TYPE)
                IP_header
            else
                done(1);
    }

// Section sub-class for a VLAN header
class VLAN_header :: Section(2) {
    struct {
        pcp : 3,
        cfi : 1,
        vid : 12,
        tpid : 16
    }
    method update = {
        fields.vid = vid,
        fields.pcp = pcp
    }
    
    method move_to_section =
    if (tpid == 0x0800)
        IP_header
    else 
        done(1);
}

const TCP_PROTO = 6;
const UDP_PROTO = 17;
// Section sub-class for an IP header
class IP_header :: Section(2, 3) {
    struct {
        version : 4,
        hdr_len : 4,
        tos : 8,
        length : 16,
        id : 16,
        flags : 3,
        offset : 13,
        ttl : 8,
        proto : 8,
        hdr_chk : 16,
        sa : 32,
        da : 32
    }
    method update = {
        fields.sa = sa,
        fields.da = da,
        fields.proto = proto,
        fields.tos = tos
    }
    method move_to_section =
    if (proto == TCP_PROTO || proto == UDP_PROTO)
        TCP_UDP_header
    else
        done(1);

    method increment_offset = hdr_len * 32;
}

// Section sub-class for a TCP or UDP header
    class TCP_UDP_header :: Section(3, 4) {
        struct {
            sp : 16,
            dp : 16
        }
        method update = {
            fields.sp = sp,
            fields.dp = dp
        }
        method move_to_section = done(0);
        method increment_offset = 0;
    }
}





