#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
/* get cores */
#include <rte_lcore.h>
#include <rte_mbuf.h>


#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
/* dump packet contents */
#include <rte_hexdump.h>
#include <rte_malloc.h>
/* paxos header definition */
#include "rte_paxos.h"
/* libpaxos learner */
#include "acceptor.h"
/* logging */
#include <rte_log.h>
/* timer event */
#include <rte_timer.h>
/* get clock cycles */
#include <rte_cycles.h>

/* number of elements in the mbuf pool */
/* NOTICE: works only with 511 */
#define NUM_MBUFS 511
/* Size of the per-core object cache */
#define MBUF_CACHE_SIZE 250
/* Maximum number of packets in sending or receiving */
#define BURST_SIZE 32

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define DEFAULT_PAXOS_PORT 34952

#define NUM_ACCEPTORS 3

#define TIMER_RESOLUTION_CYCLES 20000000ULL /* around 10ms at 2 Ghz */

#define BUFSIZE 1500

volatile bool force_quit;

static unsigned nb_ports;

/* learner timer for deliver */
//static struct rte_timer timer;
//static struct rte_timer hole_timer;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};


static const struct ether_addr ether_multicast = {
	.addr_bytes= { 0x01, 0x1b, 0x19, 0x0, 0x0, 0x0 }
};


static struct {
	uint64_t total_cycles;
	uint64_t total_pkts;
} latency_numbers;

/* structure that caches offload info for the current packet */
union tunnel_offload_info {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l4_len:8; /**< L4 Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size */
		uint64_t outer_l2_len:7; /**< outer L2 Header Length */
		uint64_t outer_l3_len:16; /**< outer L3 Header Length */
	};
} __rte_cache_aligned;

struct rte_mempool *mbuf_pool;

/**
 * Parse an ethernet header to fill the ethertype, outer_l2_len, outer_l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header.
 */
static void
parse_ethernet(struct ether_hdr *eth_hdr, union tunnel_offload_info *info,
		uint8_t *l4_proto)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	uint16_t ethertype;

	info->outer_l2_len = sizeof(struct ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == ETHER_TYPE_VLAN) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
		info->outer_l2_len  += sizeof(struct vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	switch (ethertype) {
	case ETHER_TYPE_IPv4:
		ipv4_hdr = (struct ipv4_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct ipv4_hdr);
		*l4_proto = ipv4_hdr->next_proto_id;
		break;
	case ETHER_TYPE_IPv6:
		ipv6_hdr = (struct ipv6_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct ipv6_hdr);
		*l4_proto = ipv6_hdr->proto;
		break;
	default:
		info->outer_l3_len = 0;
		*l4_proto = 0;
		break;
	}
}

static void
print_paxos_hdr(struct paxos_hdr *p)
{
	rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8,
		"{ .msgtype=%u, .inst=%u, .rnd=%u, .vrnd=%u, .acptid=%u\n",
		rte_be_to_cpu_16(p->msgtype),
		rte_be_to_cpu_32(p->inst),
		rte_be_to_cpu_16(p->rnd),
		rte_be_to_cpu_16(p->vrnd),
		rte_be_to_cpu_16(p->acptid));
}

static void
dump_paxos_message(paxos_message *m)
{
	printf("{ .type = %d, .iid = %d, .ballot = %d, .value_ballot = %d, .value = { .len = %d, val = ",
			m->type, m->u.accept.iid, m->u.accept.ballot, m->u.accept.value_ballot,
			m->u.accept.value.paxos_value_len);
	int i = 0;
	for (; i < m->u.accept.value.paxos_value_len; i++)
		printf("%02X", m->u.accept.value.paxos_value_val[i]);
	printf(" }}\n");
}

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
	if (ethertype == ETHER_TYPE_IPv4)
		return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
	else /* assume ethertype == ETHER_TYPE_IPv6 */
		return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

static int
paxos_rx_process(struct rte_mbuf *pkt, struct acceptor* acceptor)
{
	int ret = 0;
	uint8_t l4_proto = 0;
	uint16_t outer_header_len;
	union tunnel_offload_info info = { .data = 0 };
	struct udp_hdr *udp_hdr;
	struct paxos_hdr *paxos_hdr;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	parse_ethernet(phdr, &info, &l4_proto);

	if (l4_proto != IPPROTO_UDP)
		return -1;

	struct ipv4_hdr *iph = (struct ipv4_hdr *)
			((char *)phdr + info.outer_l2_len);
	udp_hdr = (struct udp_hdr *)((char *)phdr +
			info.outer_l2_len + info.outer_l3_len);

	if (udp_hdr->dst_port != rte_cpu_to_be_16(DEFAULT_PAXOS_PORT) &&
			(pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
		return -1;

	paxos_hdr = (struct paxos_hdr *)((char *)udp_hdr + sizeof(struct udp_hdr));

	if (rte_get_log_level() == RTE_LOG_DEBUG) {
		rte_hexdump(stdout, "udp", udp_hdr, sizeof(struct udp_hdr));
		rte_hexdump(stdout, "paxos", paxos_hdr, sizeof(struct paxos_hdr));
		print_paxos_hdr(paxos_hdr);
	}

	struct paxos_value *v = paxos_value_new((char *)&paxos_hdr->paxosval, 32);
	paxos_message out;
	switch (rte_be_to_cpu_16(paxos_hdr->msgtype)) {
		case PAXOS_PREPARE:
		{
			rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Paxos PREPARE message\n");
			paxos_prepare prepare = {
			.iid = rte_be_to_cpu_32(paxos_hdr->inst),
			.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
			.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
			.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
			.value = *v };
			ret = acceptor_receive_prepare(acceptor, &prepare, &out);
			break;
		}
		case PAXOS_ACCEPT:
		{
			rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Paxos ACCEPT message\n");
			paxos_accept accept = {
			.iid = rte_be_to_cpu_32(paxos_hdr->inst),
			.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
			.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
			.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
			.value = *v };
			ret = acceptor_receive_accept(acceptor, &accept, &out);
			break;
		}
		default:
			rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Unrecognized Paxos message\n");
	}

	// Acceptor has cast a vote
	if (ret == 1) {
		paxos_hdr->msgtype = rte_cpu_to_be_16(out.type);
		switch (out.type) {
			case PAXOS_PROMISE:
			{
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Promised Paxos message\n");
				dump_paxos_message(&out);
				paxos_hdr->rnd = rte_cpu_to_be_16(out.u.promise.ballot);
				paxos_hdr->vrnd = rte_cpu_to_be_16(out.u.promise.value_ballot);
				paxos_hdr->acptid = rte_cpu_to_be_16(out.u.promise.aid);
				if (out.u.promise.value.paxos_value_val != NULL)
					rte_memcpy(paxos_hdr->paxosval, out.u.promise.value.paxos_value_val, 32);
				break;
			}
			case PAXOS_ACCEPTED:
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Accepted Paxos message\n");
				paxos_hdr->acptid = rte_cpu_to_be_16(out.u.accepted.aid);
				break;
			case PAXOS_PREEMPTED:
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Preempted Paxos message\n");
				paxos_hdr->rnd = rte_cpu_to_be_16(out.u.preempted.ballot);
				paxos_hdr->vrnd = rte_cpu_to_be_16(out.u.preempted.value_ballot);
				paxos_hdr->acptid = rte_cpu_to_be_16(out.u.preempted.aid);
				break;
			default:
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Unrecognized Paxos message\n");
		}
	} else {
		rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Acceptor rejected Paxos message\n");
		outer_header_len = info.outer_l2_len + info.outer_l3_len
			+ sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);

		rte_pktmbuf_adj(pkt, outer_header_len);
	}

	pkt->l2_len = sizeof(struct ether_hdr);
	pkt->l3_len = sizeof(struct ipv4_hdr);
	pkt->l4_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
	pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
	udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct paxos_hdr));
	udp_hdr->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, pkt->ol_flags);
	return ret;

}

static uint16_t
add_timestamps(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts,
		uint16_t max_pkts __rte_unused, void *user_param)
{
	struct acceptor* acceptor = (struct acceptor*)user_param;
	unsigned i;
	uint64_t now = rte_rdtsc();

	for (i = 0; i < nb_pkts; i++) {
		pkts[i]->udata64 = now;
		paxos_rx_process(pkts[i], acceptor);
	}


	return nb_pkts;
}


static uint16_t
calc_latency(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
{
	uint64_t cycles = 0;
	uint64_t now = rte_rdtsc();
	unsigned i;

	for (i = 0; i < nb_pkts; i++) {
		cycles += now - pkts[i]->udata64;
		rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8,
				"Packet%"PRIu64", Latency = %"PRIu64" cycles\n",
				latency_numbers.total_pkts, cycles);
	}

	latency_numbers.total_cycles += cycles;
	latency_numbers.total_pkts += nb_pkts;

	if (latency_numbers.total_pkts > (100 * 1000 * 1000ULL)) {
		rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8,
		"Latency = %"PRIu64" cycles\n",
		latency_numbers.total_cycles / latency_numbers.total_pkts);
		latency_numbers.total_cycles = latency_numbers.total_pkts = 0;
	}

	return nb_pkts;
};

static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool, struct acceptor* acceptor)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	rte_eth_dev_info_get(port, &dev_info);

	rxconf = &dev_info.default_rxconf;
	txconf = &dev_info.default_txconf;

	txconf->txq_flags &= PKT_TX_IPV4;
	txconf->txq_flags &= PKT_TX_UDP_CKSUM;
	if (port >= rte_eth_dev_count())
		return -1;

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), rxconf, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), txconf);
		if (retval < 0)
			return retval;
	}

	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	rte_eth_promiscuous_enable(port);

	rte_eth_add_rx_callback(port, 0, add_timestamps, acceptor);
	rte_eth_add_tx_callback(port, 0, calc_latency, NULL);
	return 0;
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
				"\n\nSignal %d received, preparing to exit...\n", signum);
		force_quit = true;
	}
}


static void
lcore_main(void)
{
	uint8_t port;

	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
					(int) rte_socket_id())
			rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_EAL,
					"WARNING, port %u is on retmote NUMA node to "
					"polling thread.\nPerformance will "
					"not be optimal.\n", port);

	rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());


	for (;;) {
		// Check if signal is received
		if (force_quit)
			break;

		for (port = 0; port < nb_ports; port++) {
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
			if (unlikely(nb_rx == 0))
				continue;
			const uint16_t nb_tx = rte_eth_tx_burst(port, 0, bufs, nb_rx);
			if (unlikely(nb_tx < nb_rx)) {
				uint16_t buf;

				for (buf = nb_tx; buf < nb_rx; buf++)
					rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
}


__attribute__((unused))
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
	udp->dgram_len = rte_cpu_to_be_16(data_size);
	udp->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, ol_flags);
	return ol_flags;
}

__attribute__((unused))
static void
send_repeat_message(paxos_message *pm) {
	uint8_t port_id = 0;
	struct rte_mbuf *created_pkt = rte_pktmbuf_alloc(mbuf_pool);
	created_pkt->l2_len = sizeof(struct ether_hdr);
	created_pkt->l3_len = sizeof(struct ipv4_hdr);
	created_pkt->l4_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
	uint64_t ol_flags = craft_new_packet(&created_pkt, IPv4(192,168,4,95), IPv4(239,3,29,73),
			34951, 34952, sizeof(paxos_message), port_id);
	//struct udp_hdr *udp;
	size_t udp_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
	//udp  = rte_pktmbuf_mtod_offset(created_pkt, struct udp_hdr *, udp_offset);
	size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);
	paxos_message *px = rte_pktmbuf_mtod_offset(created_pkt, paxos_message *, paxos_offset);
	rte_memcpy(px, pm, sizeof(*pm));	
	//udp->dgram_cksum = get_psd_sum(udp, ETHER_TYPE_IPv4, ol_flags);
	created_pkt->ol_flags = ol_flags;
	const uint16_t nb_tx = rte_eth_tx_burst(port_id, 0, &created_pkt, 1);
	rte_pktmbuf_free(created_pkt);
	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "Send %d repeated messages\n", nb_tx);
}


/*
static __attribute__((noreturn)) int
lcore_mainloop(__attribute__((unused)) void *arg)
{
	uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
	unsigned lcore_id;

	lcore_id = rte_lcore_id();

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_TIMER,
			"Starting mainloop on core %u\n", lcore_id);

	while(1) {
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;
		if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
			rte_timer_manage();
			prev_tsc = cur_tsc;
		}
	}
}
*/

int
main(int argc, char *argv[])
{
	int acceptor_id = 2;
	uint8_t portid = 0;
	//unsigned master_core, lcore_id;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");


	nb_ports = rte_eth_dev_count();

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");

	//initialize acceptor
	struct acceptor *acceptor = acceptor_new(acceptor_id);

	/* init RTE timer library */
	rte_timer_subsystem_init();

	/* init timer structure */
	//rte_timer_init(&timer);
	//rte_timer_init(&hole_timer);

	/* load timer, every second, on a slave lcore, reloaded automatically */
	//uint64_t hz = rte_get_timer_hz();
	/* master core */
	//master_core = rte_lcore_id();
	/* slave core */
	//lcore_id = rte_get_next_lcore(master_core, 0, 1);
	//rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "lcore_id: %d\n", lcore_id);
	//rte_timer_reset(&timer, hz, PERIODICAL, lcore_id, check_deliver, learner);

	/* slave core */
	//lcore_id = rte_get_next_lcore(lcore_id, 0, 1);
	//rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "lcore_id: %d\n", lcore_id);
	//rte_timer_reset(&hole_timer, hz, PERIODICAL, lcore_id, check_holes, learner);

	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool, acceptor) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);


	/* start mainloop on every lcore */
	/*
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_mainloop, NULL, lcore_id);
	}
	*/

	lcore_main();

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "free acceptor\n");
	acceptor_free(acceptor);
	return 0;
}

