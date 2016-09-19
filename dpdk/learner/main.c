#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
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
/* timer event */
#include <rte_timer.h>
/* get clock cycles */
#include <rte_cycles.h>

/* libpaxos learner */
#include "learner.h"
/* paxos header definition */
#include "rte_paxos.h"
#include "const.h"
#include "utils.h"


struct dp_learner {
	int num_acceptors;
	int nb_learners;
	int learner_id;
	struct learner *paxos_learner;
};

#ifdef ORDER_DELIVERY
	/* learner timer for deliver */
	static struct rte_timer deliver_timer;
	static struct rte_timer hole_timer;
#endif

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};


static const struct ether_addr ether_multicast = {
	.addr_bytes= { 0x01, 0x1b, 0x19, 0x0, 0x0, 0x0 }
};


struct rte_mempool *mbuf_pool;

enum Operation {
    GET,
    SET
};

struct command {
    struct timespec ts;
    uint16_t command_id;
    enum Operation op;
    char content[32];
};


struct __attribute__((__packed__)) client_request {
    uint16_t length;
    struct sockaddr_in cliaddr;
    char content[1];
};


static void
deliver(unsigned int inst, __rte_unused char* val, size_t size, void* arg) {
	__rte_unused struct dp_learner* dl = (struct dp_learner*) arg;
	printf("deliver inst %d, size %zu\n", inst, size);
    struct client_request *req = (struct client_request*)val;
    /* Skip command ID and client address */
    // char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));
    struct command *cmd = (struct command*)(val + sizeof(struct client_request) - 1);

    if (cmd->command_id % dl->nb_learners == dl->learner_id) {
        // int n = sendto(app->paxos->sock, retval, content_length(req), 0,
        //                 (struct sockaddr *)&req->cliaddr,
        //                 sizeof(req->cliaddr));
        // if (n < 0)
        //     perror("deliver: sendto error");
        print_addr(&req->cliaddr);
    }

}

static int
paxos_rx_process(struct rte_mbuf *pkt, struct dp_learner* dl)
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

	if (udp_hdr->dst_port != rte_cpu_to_be_16(LEARNER_PORT) &&
			(pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
		return -1;

	paxos_hdr = (struct paxos_hdr *)((char *)udp_hdr + sizeof(struct udp_hdr));

	if (rte_get_log_level() == RTE_LOG_DEBUG) {
		rte_hexdump(stdout, "udp", udp_hdr, sizeof(struct udp_hdr));
		rte_hexdump(stdout, "paxos", paxos_hdr, sizeof(struct paxos_hdr));
		print_paxos_hdr(paxos_hdr);
	}

	uint16_t msgtype = rte_be_to_cpu_16(paxos_hdr->msgtype);
	switch(msgtype) {
		case PAXOS_PROMISE: {
			struct paxos_value *v = paxos_value_new((char *)paxos_hdr->paxosval, 32);
			struct paxos_promise promise = {
				.iid = rte_be_to_cpu_32(paxos_hdr->inst),
				.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
				.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
				.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
				.value = *v,
			};
			int ret;
			paxos_message pa;
			ret = learner_receive_promise(dl->paxos_learner, &promise, &pa.u.accept);
			if (ret)
				add_paxos_message(&pa, pkt, LEARNER_PORT, ACCEPTOR_PORT, ACCEPTOR_ADDR);
			break;
		}
		case PAXOS_ACCEPTED: {
			struct paxos_value *v = paxos_value_new((char *)paxos_hdr->paxosval, 32);
			struct paxos_accepted ack = {
				.iid = rte_be_to_cpu_32(paxos_hdr->inst),
				.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
				.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
				.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
				.value = *v,
			};
			if (learner_receive_accepted(dl->paxos_learner, &ack))
				deliver(ack.iid, ack.value.paxos_value_val,
						ack.value.paxos_value_len, dl);
			break;
		}
		default:
			PRINT_DEBUG("No handler for %u", msgtype);
	}



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
    struct dp_learner* dl = (struct dp_learner *)user_param;
    unsigned i;
    uint64_t now = rte_rdtsc();

    for (i = 0; i < nb_pkts; i++) {
        pkts[i]->udata64 = now;
        paxos_rx_process(pkts[i], dl);
    }
    return nb_pkts;
}

static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool, void* user_param)
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

	rte_eth_add_rx_callback(port, 0, add_timestamps, user_param);
	rte_eth_add_tx_callback(port, 0, calc_latency, user_param);
	return 0;
}


static void
lcore_main(uint8_t port)
{
	if (rte_eth_dev_socket_id(port) > 0 &&
			rte_eth_dev_socket_id(port) !=
				(int) rte_socket_id())
		rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_EAL,
				"WARNING, port %u is on retmote NUMA node to "
				"polling thread.\nPerformance will "

				"not be optimal.\n", port);
	rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "\nCore %u forwarding packets. "
				"[Ctrl+C to quit]\n", rte_lcore_id());

	for (;;) {
		// Check if signal is received
		if (force_quit)
			break;

		struct rte_mbuf *bufs[BURST_SIZE];
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
		if (unlikely(nb_rx == 0))
			continue;
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_TIMER, "Received %8d packets\n", nb_rx);

		const uint16_t nb_tx = rte_eth_tx_burst(port, 0, bufs, nb_rx);
		if (unlikely(nb_tx < nb_rx)) {
			uint16_t buf;

			for (buf = nb_tx; buf < nb_rx; buf++)
				rte_pktmbuf_free(bufs[buf]);
		}
	}
}


static void __rte_unused
check_deliver(struct rte_timer *tim,
		void *arg)
{
	struct learner* l = (struct learner *) arg;
	unsigned lcore_id = rte_lcore_id();

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "%s() on lcore_id %i\n",
			__func__, lcore_id);

	struct paxos_accepted out;
	int consensus = learner_deliver_next(l, &out);
	if (consensus)
		rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8, "consensus reached: %d\n",
			consensus);
	/* this timer is automatically reloaded until we decide to stop it */
	if (force_quit)
		rte_timer_stop(tim);
}

static void __rte_unused
check_holes(struct rte_timer *tim, void *arg)
{
	struct dp_learner *dl = (struct dp_learner *) arg;
	unsigned lcore_id = rte_lcore_id();

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "%s() on lcore_id %i\n",
				__func__, lcore_id);
	struct rte_mbuf *created_pkt = rte_pktmbuf_alloc(mbuf_pool);
	unsigned from_inst;
	unsigned to_inst;
	if (learner_has_holes(dl->paxos_learner, &from_inst, &to_inst)) {
		paxos_log_debug("Learner has holes from %d to %d\n", from_inst, to_inst);
        unsigned iid;
        for (iid = from_inst; iid < to_inst; iid++) {
            paxos_message out;
            out.type = PAXOS_PREPARE;
            learner_prepare(dl->paxos_learner, &out.u.prepare, iid);
			add_paxos_message(&out, created_pkt, LEARNER_PORT, ACCEPTOR_PORT, ACCEPTOR_ADDR);
        }
	}
	/* this timer is automatically reloaded until we decide to stop it */
	if (force_quit)
		rte_timer_stop(tim);
}

int
main(int argc, char *argv[])
{
	uint8_t portid = 0;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	if (rte_get_log_level() == RTE_LOG_DEBUG) {
		paxos_config.verbosity = PAXOS_LOG_DEBUG;
	}

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");

	//initialize learner
	struct dp_learner dp_learner = {
		.num_acceptors = 1,
		.nb_learners = 1,
		.learner_id = 0,
	};
	dp_learner.paxos_learner = learner_new(dp_learner.num_acceptors);
	learner_set_instance_id(dp_learner.paxos_learner, 0);

	/* load deliver_timer, every second, on a slave lcore, reloaded automatically */
	uint64_t hz = rte_get_timer_hz();

	/* Call rte_timer_manage every 10ms */
	TIMER_RESOLUTION_CYCLES = hz / 100;

#ifdef ORDER_DELIVERY
	unsigned master_core = rte_lcore_id();
	unsigned lcore_id;
	/* init RTE timer library */
	rte_timer_subsystem_init();
	/* init timer structure */
	rte_timer_init(&deliver_timer);
	rte_timer_init(&hole_timer);
	/* slave core */
	lcore_id = rte_get_next_lcore(master_core, 0, 1);
	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "lcore_id: %d\n", lcore_id);
	rte_timer_reset(&deliver_timer, hz, PERIODICAL, lcore_id, check_deliver, &dp_learner);
	rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);
	/* slave core */
	lcore_id = rte_get_next_lcore(lcore_id, 0, 1);
	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "lcore_id: %d\n", lcore_id);
	rte_timer_reset(&hole_timer, hz, PERIODICAL, lcore_id, check_holes, &dp_learner);
	rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);
#endif

	if (port_init(portid, mbuf_pool, &dp_learner) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);

	lcore_main(portid);

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "free learner\n");
	learner_free(dp_learner.paxos_learner);
	return 0;
}

