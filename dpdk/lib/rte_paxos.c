#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "rte_paxos.h"
#include "const.h"

void print_paxos_hdr(struct paxos_hdr *p)
{
    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8,
        "{ .msgtype=%u, .inst=%u, .rnd=%u, .vrnd=%u, .acptid=%u\n",
        rte_be_to_cpu_16(p->msgtype),
        rte_be_to_cpu_32(p->inst),
        rte_be_to_cpu_16(p->rnd),
        rte_be_to_cpu_16(p->vrnd),
        rte_be_to_cpu_16(p->acptid));
}

uint16_t get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
    if (ethertype == ETHER_TYPE_IPv4)
        return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
    else /* assume ethertype == ETHER_TYPE_IPv6 */
        return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

uint64_t craft_new_packet(struct rte_mbuf **created_pkt, uint32_t srcIP, uint32_t dstIP,
        uint16_t sport, uint16_t dport, size_t data_size, uint8_t output_port)
{
    uint64_t ol_flags = 0;
    size_t pkt_size = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
        sizeof(struct udp_hdr) + data_size;
    (*created_pkt)->data_len = pkt_size;
    (*created_pkt)->pkt_len = pkt_size;
    struct ether_hdr *eth;
    eth = rte_pktmbuf_mtod(*created_pkt, struct ether_hdr*);
    /* set packet s_addr using mac address of output port */
    rte_eth_macaddr_get(output_port, &eth->s_addr);
    /* Set multicast address 01-1b-19-00-00-00 */
    eth->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
    struct ipv4_hdr *iph;
    iph = (struct ipv4_hdr *)rte_pktmbuf_mtod_offset(*created_pkt, struct ipv4_hdr*,
            sizeof(struct ether_hdr));
    iph->src_addr = rte_cpu_to_be_32(srcIP);
    iph->dst_addr = rte_cpu_to_be_32(dstIP);
    iph->version_ihl = 0x45;
    iph->hdr_checksum = 0;
    ol_flags |= PKT_TX_IPV4;
    ol_flags |= PKT_TX_IP_CKSUM;
    iph->total_length = rte_cpu_to_be_16( sizeof(struct ipv4_hdr) +
            sizeof(struct udp_hdr) + sizeof(struct paxos_message));
    iph->next_proto_id = IPPROTO_UDP;
    struct udp_hdr *udp;
    udp = (struct udp_hdr *)((unsigned char*)iph + sizeof(struct ipv4_hdr));
    udp->src_port = rte_cpu_to_be_16(sport);
    udp->dst_port = rte_cpu_to_be_16(dport);
    ol_flags |= PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, ol_flags);
    udp->dgram_len = rte_cpu_to_be_16(data_size);
    return ol_flags;
}

void add_paxos_message(struct paxos_message *pm, struct rte_mbuf *created_pkt)
{
    uint8_t port_id = 0;
    // struct rte_mbuf *created_pkt = rte_pktmbuf_alloc(mbuf_pool);
    created_pkt->l2_len = sizeof(struct ether_hdr);
    created_pkt->l3_len = sizeof(struct ipv4_hdr);
    created_pkt->l4_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
    uint64_t ol_flags = craft_new_packet(&created_pkt, IPv4(192,168,4,95),
                        IPv4(239,3,29,73), PROPOSER_PORT, ACCEPTOR_PORT,
                        sizeof(struct paxos_message), port_id);
    size_t udp_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);

    struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt, struct paxos_hdr *, paxos_offset);
    px->msgtype = rte_cpu_to_be_16(pm->type);
    px->inst = rte_cpu_to_be_32(pm->u.accept.iid);
    px->rnd = rte_cpu_to_be_16(pm->u.accept.ballot);
    px->vrnd = rte_cpu_to_be_16(pm->u.accept.value_ballot);
    px->acptid = 0;
    px->value_len = rte_cpu_to_be_16(pm->u.accept.value.paxos_value_len);
    rte_memcpy(px->paxosval, pm->u.accept.value.paxos_value_val, pm->u.accept.value.paxos_value_len);
    created_pkt->ol_flags = ol_flags;
}

void send_batch(struct rte_mbuf **mbufs, int count, int port_id)
{
    uint16_t nb_tx;
    do {
        nb_tx = rte_eth_tx_burst(port_id, 0, mbufs, count);
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "Send %d messages\n", nb_tx);
    } while (nb_tx == count);
}