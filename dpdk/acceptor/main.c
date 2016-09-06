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
/* paxos header definition */
#include "rte_paxos.h"
/* libpaxos acceptor */
#include "acceptor.h"
/* logging */
#include <rte_log.h>
/* timer event */
#include <rte_timer.h>
/* get clock cycles */
#include <rte_cycles.h>

#include "utils.h"
#include "const.h"

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

struct rte_mempool *mbuf_pool;

__attribute((unused))
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

__attribute((unused))
static void
dump_prepare_message(paxos_prepare *m)
{
	printf("{ .type = PREPARE, .iid = %d, .ballot = %d, .value_ballot = %d\n",
			m->iid, m->ballot, m->value_ballot);
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

	if (udp_hdr->dst_port != rte_cpu_to_be_16(ACCEPTOR_PORT) &&
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
			//dump_prepare_message(&prepare);
			ret = acceptor_receive_prepare(acceptor, &prepare, &out);
			udp_hdr->src_port = rte_cpu_to_be_16(ACCEPTOR_PORT);
			udp_hdr->dst_port = rte_cpu_to_be_16(PROPOSER_PORT);
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
			udp_hdr->src_port = rte_cpu_to_be_16(ACCEPTOR_PORT);
			udp_hdr->dst_port = rte_cpu_to_be_16(LEARNER_PORT);
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
				//dump_paxos_message(&out);
				paxos_hdr->inst = rte_cpu_to_be_32(out.u.promise.iid);
				paxos_hdr->rnd = rte_cpu_to_be_16(out.u.promise.ballot);
				paxos_hdr->vrnd = rte_cpu_to_be_16(out.u.promise.value_ballot);
				paxos_hdr->acptid = rte_cpu_to_be_16(out.u.promise.aid);
				paxos_hdr->value_len = rte_cpu_to_be_16(out.u.promise.value.paxos_value_len);
				if (out.u.promise.value.paxos_value_len != 0)
					rte_memcpy(paxos_hdr->paxosval, out.u.promise.value.paxos_value_val,
							out.u.promise.value.paxos_value_len);
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
	}

	latency_numbers.total_cycles += cycles;
	latency_numbers.total_pkts += nb_pkts;

	if (latency_numbers.total_pkts > (1 * 1000 * 1000ULL)) {
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
lcore_main(uint8_t port)
{
	if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) != (int) rte_socket_id()) {
		rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_EAL,
				"WARNING, port %u is on retmote NUMA node to "
				"polling thread.\nPerformance will "
				"not be optimal.\n", port);
	}

	rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "\nCore %u forwarding packets."
				"[Ctrl+C to quit]\n", rte_lcore_id());

	for (;;) {
		// Check if signal is received
		if (force_quit)
			break;

		struct rte_mbuf *bufs[BURST_SIZE];
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
		if (unlikely(nb_rx == 0))
			continue;

			uint16_t nb_tx = rte_eth_tx_burst(port, 0, bufs, nb_rx);
			while (unlikely(nb_tx < nb_rx)) {
				nb_tx = rte_eth_tx_burst(port, 0, bufs, nb_rx);
			}
	}
}



int
main(int argc, char *argv[])
{
	int acceptor_id = 0;
	uint8_t portid = 0;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");



	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");

	//initialize acceptor
	struct acceptor *acceptor = acceptor_new(acceptor_id);

	if (port_init(portid, mbuf_pool, acceptor) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);


	lcore_main(portid);

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "free acceptor\n");
	acceptor_free(acceptor);
	return 0;
}

