#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_cycles.h>
#include <rte_timer.h>
#include "rte_paxos.h"
#include "const.h"

#include <rte_hexdump.h>

struct {
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;

union {
    uint64_t as_int;
    struct ether_addr as_addr;
} dst_eth_addr;

/*
 * Construct Ethernet multicast address from IPv4 multicast address.
 * Citing RFC 1112, section 6.4:
 * "An IP host group address is mapped to an Ethernet multicast address
 * by placing the low-order 23-bits of the IP address into the low-order
 * 23 bits of the Ethernet multicast address 01-00-5E-00-00-00 (hex)."
 */
#define ETHER_ADDR_FOR_IPV4_MCAST(x)    \
        (rte_cpu_to_be_64(0x01005e000000ULL | ((x) & 0x7fffff)) >> 16)


void print_paxos_hdr(struct paxos_hdr *p)
{
    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8,
        "{ .msgtype=%u, .inst=%u, .rnd=%u, .vrnd=%u, .acptid=%u, .value_size=%u\n",
        rte_be_to_cpu_16(p->msgtype),
        rte_be_to_cpu_32(p->inst),
        rte_be_to_cpu_16(p->rnd),
        rte_be_to_cpu_16(p->vrnd),
        rte_be_to_cpu_16(p->acptid),
        rte_be_to_cpu_32(p->value_len)
        );
}

uint16_t get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
    if (ethertype == ETHER_TYPE_IPv4)
        return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
    else /* assume ethertype == ETHER_TYPE_IPv6 */
        return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

void craft_new_packet(struct rte_mbuf **created_pkt, uint32_t srcIP, uint32_t dstIP,
        uint16_t sport, uint16_t dport, size_t data_size, uint8_t output_port)
{
    size_t pkt_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
        sizeof(struct udp_hdr) + data_size;
    (*created_pkt)->data_len = pkt_size;
    (*created_pkt)->pkt_len = pkt_size;
    struct ether_hdr *eth;
    eth = rte_pktmbuf_mtod(*created_pkt, struct ether_hdr*);
    /* set packet s_addr using mac address of output port */
    rte_eth_macaddr_get(output_port, &eth->s_addr);
    /*
     * Assume all dstIP is multicast address. Then the destination MAC address
     * need to be recomputed.
     */
    dst_eth_addr.as_int = ETHER_ADDR_FOR_IPV4_MCAST(dstIP);
    ether_addr_copy(&dst_eth_addr.as_addr, &eth->d_addr);
    eth->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

    struct ipv4_hdr *iph;
    iph = (struct ipv4_hdr *)rte_pktmbuf_mtod_offset(*created_pkt, struct ipv4_hdr*,
            sizeof(struct ether_hdr));
    iph->total_length = rte_cpu_to_be_16(sizeof(struct ipv4_hdr) +
            sizeof(struct udp_hdr) + data_size);
    iph->version_ihl = 0x45;
    iph->time_to_live = 64;
    iph->packet_id = rte_cpu_to_be_16(rte_rdtsc());
    iph->fragment_offset = rte_cpu_to_be_16(IPV4_HDR_DF_FLAG);
    iph->next_proto_id = IPPROTO_UDP;
    iph->hdr_checksum = 0;
    iph->src_addr = rte_cpu_to_be_32(srcIP);
    iph->dst_addr = rte_cpu_to_be_32(dstIP);
    struct udp_hdr *udp;
    udp = (struct udp_hdr *)((unsigned char*)iph + sizeof(struct ipv4_hdr));
    udp->src_port = rte_cpu_to_be_16(sport);
    udp->dst_port = rte_cpu_to_be_16(dport);
    udp->dgram_len = rte_cpu_to_be_16(sizeof(struct udp_hdr) + data_size);

    (*created_pkt)->l2_len = sizeof(struct ether_hdr);
    (*created_pkt)->l3_len = sizeof(struct ipv4_hdr);
    (*created_pkt)->l4_len = sizeof(struct udp_hdr) + data_size;
    (*created_pkt)->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, (*created_pkt)->ol_flags);
}

void add_paxos_message(struct paxos_message *pm, struct rte_mbuf *created_pkt,
                        uint16_t sport, uint16_t dport, uint32_t dstIP)
{
    uint8_t port_id = 0;
    size_t udp_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);

    struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt,
                                struct paxos_hdr *, paxos_offset);
    px->msgtype = rte_cpu_to_be_16(pm->type);
    px->inst = rte_cpu_to_be_32(pm->u.accept.iid);
    px->rnd = rte_cpu_to_be_16(pm->u.accept.ballot);
    px->vrnd = rte_cpu_to_be_16(pm->u.accept.value_ballot);
    px->acptid = rte_cpu_to_be_16(pm->u.accept.aid);
    px->value_len = rte_cpu_to_be_32(pm->u.accept.value.paxos_value_len);
    rte_memcpy(px->paxosval, pm->u.accept.value.paxos_value_val,
                pm->u.accept.value.paxos_value_len);
    craft_new_packet(&created_pkt, IPv4(192,168,4,97),
                        dstIP, sport, dport,
                        sizeof(struct paxos_message) +
                        pm->u.accept.value.paxos_value_len, port_id);
}

void send_batch(struct rte_mbuf **mbufs, int count, int port_id)
{
    uint16_t nb_tx;
    do {
        nb_tx = rte_eth_tx_burst(port_id, 0, mbufs, count);
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "Send %d messages\n", nb_tx);
    } while (nb_tx == count);
}

uint16_t calc_latency(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
        struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
{
    uint64_t cycles = 0;
    uint64_t now = rte_rdtsc();
    unsigned i;

    for (i = 0; i < nb_pkts; i++) {
        cycles += now - pkts[i]->udata64;
    }

    latency_numbers.total_cycles += cycles;
    latency_numbers.total_pkts += nb_pkts;

    if (latency_numbers.total_pkts >= ( 1000 * 1000ULL)) {
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
        "avg. latency = %"PRIu64" cycles\n",
        latency_numbers.total_cycles / latency_numbers.total_pkts);
        latency_numbers.total_cycles = latency_numbers.total_pkts = 0;
    }
    return nb_pkts;
}

int
check_timer_expiration(__attribute__((unused)) void *arg)
{
    uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
    unsigned lcore_id;

    lcore_id = rte_lcore_id();

    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_TIMER,
            "Starting %s on core %u\n", __func__, lcore_id);

    while(!force_quit) {
        cur_tsc = rte_rdtsc();
        diff_tsc = cur_tsc - prev_tsc;
        if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
            rte_timer_manage();
            prev_tsc = cur_tsc;
        }
    }
    return 0;
}

void
print_addr(struct sockaddr_in* s) {
    printf("address %s, port %d\n", inet_ntoa(s->sin_addr), ntohs(s->sin_port));
}