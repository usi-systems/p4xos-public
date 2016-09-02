#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

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
/* logging */
#include <rte_log.h>
/* paxos header definition */
#include "rte_paxos.h"
#include "utils.h"
#include "const.h"


static unsigned nb_ports;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};

static struct {
	uint64_t total_cycles;
	uint64_t total_pkts;
} latency_numbers;


struct rte_mempool *mbuf_pool;

struct coordinator {
	uint32_t cur_inst; /* Paxos instance */
};




static int
paxos_rx_process(struct rte_mbuf *pkt, struct coordinator *cord)
{
	int ret = 0;
	uint8_t l4_proto = 0;
	union tunnel_offload_info info = { .data = 0 };
	struct ipv4_hdr *iph;
	struct udp_hdr *udp_hdr;
	struct paxos_hdr *paxos_hdr;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	parse_ethernet(phdr, &info, &l4_proto);

	if (l4_proto != IPPROTO_UDP)
		return -1;

	iph = (struct ipv4_hdr *) ((char *)phdr + info.outer_l2_len);
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


	paxos_hdr->inst = rte_cpu_to_be_32(cord->cur_inst++);
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
	struct coordinator* cord = (struct coordinator*)user_param;
	unsigned i;
	uint64_t now = rte_rdtsc();

	for (i = 0; i < nb_pkts; i++) {
		pkts[i]->udata64 = now;
		paxos_rx_process(pkts[i], cord);
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
port_init(uint8_t port, struct rte_mempool *mbuf_pool, struct coordinator* cord)
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

	rte_eth_add_rx_callback(port, 0, add_timestamps, cord);
	rte_eth_add_tx_callback(port, 0, calc_latency, NULL);
	return 0;
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


int
main(int argc, char *argv[])
{
	uint8_t portid = 0;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	struct coordinator c = { .cur_inst = 0 };
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


	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool, &c) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);

	lcore_main();

	return 0;
}

