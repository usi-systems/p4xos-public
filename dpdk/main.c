#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
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
#include "learner.h"
/* logging */
#include <rte_log.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define DEFAULT_PAXOS_PORT 34952

#define NUM_ACCEPTORS 3

volatile bool force_quit;

static unsigned nb_ports;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
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

static int
paxos_rx_process(struct rte_mbuf *pkt, struct learner* l)
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

	struct paxos_value *v = paxos_value_new((char *)paxos_hdr->paxosval, 32);
	struct paxos_accepted ack = {
			.iid = rte_be_to_cpu_32(paxos_hdr->inst),
			.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
			.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
			.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
			.value = *v };

	struct paxos_accepted out;
	learner_receive_accepted(l, &ack);
	int consensus = learner_deliver_next(l, &out);
	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "consensus reached: %d\n", consensus);

	outer_header_len = info.outer_l2_len + info.outer_l3_len
		+ sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);

	rte_pktmbuf_adj(pkt, outer_header_len);


	return ret;

}

static uint16_t
add_timestamps(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts,
		uint16_t max_pkts __rte_unused, void *user_param)
{
	struct learner* learner = (struct learner *)user_param;
	unsigned i;
	uint64_t now = rte_rdtsc();

	for (i = 0; i < nb_pkts; i++) {
		pkts[i]->udata64 = now;
		paxos_rx_process(pkts[i], learner);
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
port_init(uint8_t port, struct rte_mempool *mbuf_pool, struct learner* learner)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned) port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	rte_eth_promiscuous_enable(port);

	rte_eth_add_rx_callback(port, 0, add_timestamps, learner);
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
			const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
					bufs, nb_rx);
			if (unlikely(nb_tx < nb_rx)) {
				uint16_t buf;

				for (buf = nb_tx; buf < nb_rx; buf++)
					rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	uint8_t portid = 0;

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	rte_set_log_level(RTE_LOG_DEBUG);

	nb_ports = rte_eth_dev_count();
	if (nb_ports < 2 || (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);

	//initialize learner
	struct learner *learner = learner_new(NUM_ACCEPTORS);
	learner_set_instance_id(learner, -1);

	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool, learner) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);


	if (rte_lcore_count() > 1)
		rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_EAL, "\nWARNING: Too much enabled lcores -"
			"App use only 1 lcore\n");

	lcore_main();

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "free learner\n");
	learner_free(learner);
	return 0;
}

