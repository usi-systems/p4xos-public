#ifndef _SIMPLE_SUME_P4_
#define _SIMPLE_SUME_P4_

// File "simple_sume_switch.p4"
// NetFPGA SUME P4 Switch declaration
// core library needed for packet_in definition
#include <core.p4>

// one-hot encoded: {DMA, NF3, DMA, NF2, DMA, NF1, DMA, NF0}
typedef bit<8> port_t;

// digest metadata to send to CPU
struct digest_metadata_t {
    bit<8> src_port;
    bit<48> eth_src_addr;
    bit<24> unused;
}

///* standard sume switch metadata */
//struct sume_metadata_t {
//    bit<16> pkt_len; // unsigned int
//    port_t src_port; // one-hot encoded: {DMA, NF3, DMA, NF2, DMA, NF1, DMA, NF0}
//    port_t dst_port; // one-hot encoded: {DMA, NF3, DMA, NF2, DMA, NF1, DMA, NF0}
//    bit<1> drop;
//    bit<1> send_dig_to_cpu; // send digest_data to CPU
//    digest_metadata_t digest_data; // data to be sent to the CPU rather than the full packet
//    bit<14> unused;
//}

/* standard sume switch metadata */
struct sume_metadata_t {
    bit<16> pkt_len; // unsigned int
    port_t src_port; // one-hot encoded: {DMA, NF3, DMA, NF2, DMA, NF1, DMA, NF0}
    port_t dst_port; // one-hot encoded: {DMA, NF3, DMA, NF2, DMA, NF1, DMA, NF0}
    bit<8> drop;
    bit<8> send_dig_to_cpu; // send digest_data to CPU
    digest_metadata_t digest_data; // data to be sent to the CPU rather than the full packet
}

/* Prototypes for all programmable blocks */
/**
 * Programmable parser.
 * @param b input packet
 * @param <H> type of headers; defined by user
 * @param parsedHeaders headers constructed by parser
 * @param <M> type of metadata; defined by user
 * @param metadata; metadata constructed by parser
 * @param sume_metadata; standard metadata for the sume switch
 */
parser Parser<H, M>(packet_in b,
                    out H parsedHeaders,
                    out M user_metadata,
                    inout sume_metadata_t sume_metadata);

/**
 * Match-action pipeline
 * @param <H> type of input and output headers
 * @param parsedHeaders; headers received from the parser and sent to the deparser
 * @param <M> type of input and output user metadata
 * @param user_metadata; metadata defined by the user
 * @param sume_metadata; standard metadata for the sume switch
 */
control Pipe<H, M>(inout H parsedHeaders,
                   inout M user_metadata,
                   inout sume_metadata_t sume_metadata);

/**
 * Switch deparser.
 * @param b output packet
 * @param <H> type of headers; defined by user
 * @param parsedHeaders headers for output packet
 * @param <M> type of metadata; defined by user
 * @param user_metadata; defined by user
 * @param sume_metadata; standard metadata for the sume switch
 */
control Deparser<H, M>(packet_out b,
                       in H parsedHeaders,
                       in M user_metadata,
                       inout sume_metadata_t sume_metadata);

/**
 * Top-level package declaration - must be instantiated by user.
 * The arguments to the package indicate blocks that
 * must be instantiated by the user.
 * @param <H> user-defined type of the headers processed.
 */
package SimpleSumeSwitch<H, M>(Parser<H, M> p,
                               Pipe<H, M> map,
                               Deparser<H, M> d);

#endif  /* _SIMPLE_SUME_P4_ */