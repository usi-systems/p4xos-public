#include <stdint.h>
#include <inttypes.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_log.h>
#include "dpdk_proposer.h"
#include "const.h"
#include "utils.h"

static uint64_t
craft_new_packet(struct rte_mbuf **created_pkt, uint32_t srcIP, uint32_t dstIP,
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
    ether_addr_copy(&ether_multicast, &eth->d_addr);
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
            sizeof(struct udp_hdr) + sizeof(paxos_message));
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

void send_paxos_message(paxos_message *pm) {
    uint8_t port_id = 0;
    struct rte_mbuf *created_pkt = rte_pktmbuf_alloc(mbuf_pool);
    created_pkt->l2_len = sizeof(struct ether_hdr);
    created_pkt->l3_len = sizeof(struct ipv4_hdr);
    created_pkt->l4_len = sizeof(struct udp_hdr) + sizeof(paxos_message);
    uint64_t ol_flags = craft_new_packet(&created_pkt, IPv4(192,168,4,95), IPv4(239,3,29,73),
            PROPOSER_PORT, ACCEPTOR_PORT, sizeof(paxos_message), port_id);
    //struct udp_hdr *udp;
    size_t udp_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    //udp  = rte_pktmbuf_mtod_offset(created_pkt, struct udp_hdr *, udp_offset);
    size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);
    struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt, struct paxos_hdr *, paxos_offset);
    px->msgtype = rte_cpu_to_be_16(pm->type);
    px->inst = rte_cpu_to_be_32(pm->u.accept.iid);
    px->inst = rte_cpu_to_be_32(pm->u.accept.iid);
    px->rnd = rte_cpu_to_be_16(pm->u.accept.ballot);
    px->vrnd = rte_cpu_to_be_16(pm->u.accept.value_ballot);
    px->acptid = 0;
    rte_memcpy(px->paxosval, pm->u.accept.value.paxos_value_val, pm->u.accept.value.paxos_value_len);
    created_pkt->ol_flags = ol_flags;
    const uint16_t nb_tx = rte_eth_tx_burst(port_id, 0, &created_pkt, 1);
    rte_pktmbuf_free(created_pkt);
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "Send %d messages\n", nb_tx);
}

void proposer_preexecute(struct proposer *p)
{
    int i;
    paxos_message msg;
    msg.type = PAXOS_PREPARE;
    int count = PREEXEC_WINDOW - proposer_prepared_count(p);
    if (count <= 0)
        return;
    for (i = 0; i < count; i++) {
        proposer_prepare(p, &msg.u.prepare);
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
            "%s Prepare instance %d ballot %d\n",
            __func__, msg.u.prepare.iid, msg.u.prepare.ballot);
        send_paxos_message(&msg);
    }

}


void try_accept(struct proposer *p)
{
    paxos_message msg;
    msg.type = PAXOS_ACCEPT;
    while (proposer_accept(p, &msg.u.accept)) {
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
            "Send ACCEPT for instance %d\n", msg.u.accept.iid);
        send_paxos_message(&msg);
    }
    proposer_preexecute(p);
}


void proposer_handle_promise(struct proposer *p, struct paxos_promise *promise)
{
    struct paxos_message msg;
    msg.type = PAXOS_PREPARE;
    int preempted = proposer_receive_promise(p, promise, &msg.u.prepare);
    if (preempted) {
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
            "%s Prepare instance %d ballot %d\n",
            __func__, msg.u.prepare.iid, msg.u.prepare.ballot);
        send_paxos_message(&msg);
    }
    try_accept(p);
}

void proposer_handle_accepted(struct proposer *p, struct paxos_accepted *ack)
{
    if (proposer_receive_accepted(p, ack)) {
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
            "Reach Quorum. Instance: %d\n",
            ack->iid);
            rte_atomic32_add(&stat, 1);
        /* Assume Receiving a new command */
        proposer_propose(p, command, 11);
        try_accept(p);
    }
}