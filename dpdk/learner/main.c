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
#include "args.h"

#include "leveldb_context.h"
#include "message.h"

#define NS_PER_S 1000000000

#define BURST_TX_DRAIN_NS 100
#define FAKE_ADDR IPv4(192,168,4,198)

struct dp_learner {
	int num_acceptors;
	int nb_learners;
	int learner_id;
    int nb_pkt_buf;
    int enable_leveldb;
    struct leveldb_ctx *leveldb;
	struct learner *paxos_learner;
    struct rte_mempool *mbuf_pool;
	struct rte_mempool *mbuf_tx;
    struct rte_eth_dev_tx_buffer *tx_buffer;
    struct rte_mbuf *tx_mbufs[BURST_SIZE];
};



// Mac address of eth0 node95
static const struct ether_addr mac95 = {
	.addr_bytes= { 0x0c, 0xc4, 0x7a, 0xba, 0xc6, 0xc6 }
};

// Mac address of eth10 node96
static const struct ether_addr mac96 = {
	.addr_bytes= { 0x0c, 0xc4, 0x7a, 0xba, 0xc2, 0x40 }
};

// Mac address of eth21 node97
static const struct ether_addr mac97 = {
	.addr_bytes= { 0x0c, 0xc4, 0x7a, 0xba, 0xc6, 0xad }
};

// Mac address of eth1 node98
static const struct ether_addr mac98 = {
	.addr_bytes= { 0x0c, 0xc4, 0x7a, 0xba, 0xc0, 0x08 }
};

static rte_atomic32_t tx_counter = RTE_ATOMIC32_INIT(0);
static rte_atomic32_t rx_counter = RTE_ATOMIC32_INIT(0);
static rte_atomic64_t tot_cycles = RTE_ATOMIC64_INIT(0);

static rte_atomic32_t consensus_counter = RTE_ATOMIC32_INIT(0);

static uint32_t at_second;
static uint32_t dropped;
static uint64_t sample_cycle;

#ifdef ORDER_DELIVERY
	/* learner timer for deliver */
	static struct rte_timer deliver_timer;
	static struct rte_timer hole_timer;
#endif
static struct rte_timer timer;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};


static void
reset_tx_mbufs(struct dp_learner* dl, unsigned nb_tx)
{
    rte_atomic32_add(&tx_counter, nb_tx);
    rte_pktmbuf_alloc_bulk(dl->mbuf_tx, dl->tx_mbufs, nb_tx);
    dl->nb_pkt_buf = 0;
}


static int
dp_learner_send(struct dp_learner* dl,
					char* data, size_t size, struct sockaddr_in* dest) {

     struct rte_mbuf *pkt = dl->tx_mbufs[dl->nb_pkt_buf++];
	struct ipv4_hdr *iph;
	struct udp_hdr *udp_hdr;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

    int l2_len = sizeof(struct ether_hdr);
    int l3_len = sizeof(struct ipv4_hdr);
    int l4_len = sizeof(struct udp_hdr);

    iph = (struct ipv4_hdr *) ((char *)phdr + l2_len);
    udp_hdr = (struct udp_hdr *)((char *)iph + l3_len);

    char *datagram = rte_pktmbuf_mtod_offset(pkt,
                                char *, l2_len + l3_len + l4_len);
    rte_memcpy(datagram, data, size);

    phdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
    // set src MAC address
	rte_eth_macaddr_get(0, &phdr->s_addr);
	switch (dest->sin_addr.s_addr) {
		case 0x5f04a8c0:
			ether_addr_copy(&mac95, &phdr->d_addr);
			break;
		case 0x6004a8c0:
			ether_addr_copy(&mac96, &phdr->d_addr);
			break;
		case 0x6104a8c0:
			ether_addr_copy(&mac97, &phdr->d_addr);
			break;
		case 0x6204a8c0:
			ether_addr_copy(&mac98, &phdr->d_addr);
			break;
		default:
			PRINT_INFO("Unknown host %x", dest->sin_addr.s_addr);
            print_addr(dest);
	}

    iph->total_length = rte_cpu_to_be_16(l3_len + l4_len + size);
    iph->version_ihl = 0x45;
    iph->time_to_live = 64;
    iph->packet_id = rte_cpu_to_be_16(rte_rdtsc());
    iph->fragment_offset = rte_cpu_to_be_16(IPV4_HDR_DF_FLAG);
    iph->next_proto_id = IPPROTO_UDP;
    iph->hdr_checksum = 0;
    iph->src_addr = rte_cpu_to_be_32(FAKE_ADDR);
    iph->dst_addr = dest->sin_addr.s_addr;  // Already in network byte order
	udp_hdr->dst_port = dest->sin_port; // Already in network byte order
    udp_hdr->src_port = rte_cpu_to_be_16(LEARNER_PORT);
	udp_hdr->dgram_len = rte_cpu_to_be_16(l4_len + size);
	pkt->l2_len = l2_len;
	pkt->l3_len = l3_len;
	pkt->l4_len = l4_len + size;
    pkt->data_len = l2_len + l3_len + l4_len + size;
    pkt->pkt_len = l2_len + l3_len + l4_len + size;
	pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
	udp_hdr->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, pkt->ol_flags);
    int nb_tx = rte_eth_tx_buffer(0, 0, dl->tx_buffer, pkt);
	if (nb_tx) {
        reset_tx_mbufs(dl, nb_tx);
        rte_atomic32_add(&tx_counter, nb_tx);
    }

	return 0;
}

static int
deliver(unsigned int __rte_unused inst, __rte_unused char* val,
			__rte_unused size_t size, void* arg) {

	struct dp_learner* dl = (struct dp_learner*) arg;
    struct client_request *req = (struct client_request*)val;
    /* Skip command ID and client address */
    char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));
    struct command *cmd = (struct command*)(val + sizeof(struct client_request) - 1);

    if (dl->enable_leveldb) {
        char *key = cmd->content;
        if (cmd->op == SET) {
            char *value = cmd->content + 16;
            PRINT_DEBUG("SET(%s, %s)", key, value);
            int res = add_entry(dl->leveldb, 0, key, 16, value, 16);
            if (res) {
                fprintf(stderr, "Add entry failed.\n");
            }
        }
        else if (cmd->op == GET) {
            /* check if the value is stored */
            char *stored_value = NULL;
            size_t vsize = 0;
            int res = get_value(dl->leveldb, key, 16, &stored_value, &vsize);
            if (res) {
                fprintf(stderr, "get value failed.\n");
            }
            else {
                if (stored_value != NULL) {
                    PRINT_DEBUG("Stored value %s, size %zu", stored_value, vsize);
                    free(stored_value);
                }
            }
        }
    }

    /* TEST: only the first learner responds */
    // if (cmd->command_id % dl->nb_learners == dl->learner_id) {
    if (dl->learner_id == 0) {
        // print_addr(&req->cliaddr);
        return dp_learner_send(dl, retval, content_length(req), &req->cliaddr);
    }
    return -1;
}

static int
paxos_rx_process(struct rte_mbuf *pkt, struct dp_learner* dl)
{
	int ret = -1;
	struct udp_hdr *udp_hdr;
	struct paxos_hdr *paxos_hdr;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

    int l2_len = sizeof(struct ether_hdr);
    int l3_len = sizeof(struct ipv4_hdr);
    // int l4_len = sizeof(struct udp_hdr);

    struct ipv4_hdr *iph = (struct ipv4_hdr *) ((char *)phdr + l2_len);
    if (rte_log_get_global_level() == RTE_LOG_DEBUG)
        rte_hexdump(stdout, "ip", iph, sizeof(struct ipv4_hdr));

    if (iph->next_proto_id != IPPROTO_UDP)
        return -1;

    udp_hdr = (struct udp_hdr *)((char *)iph + l3_len);

	if (udp_hdr->dst_port != rte_cpu_to_be_16(LEARNER_PORT) &&
			(pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
		return -1;

	paxos_hdr = (struct paxos_hdr *)((char *)udp_hdr + sizeof(struct udp_hdr));

	if (rte_log_get_global_level() == RTE_LOG_DEBUG) {
		rte_hexdump(stdout, "udp", udp_hdr, sizeof(struct udp_hdr));
		rte_hexdump(stdout, "paxos", paxos_hdr, sizeof(struct paxos_hdr));
		print_paxos_hdr(paxos_hdr);
	}

	uint16_t msgtype = rte_be_to_cpu_16(paxos_hdr->msgtype);
	switch(msgtype) {
		case PAXOS_PROMISE: {
            int vsize = rte_be_to_cpu_32(paxos_hdr->value_len);
            struct paxos_value *v = paxos_value_new((char *)paxos_hdr->paxosval, vsize);
			struct paxos_promise promise = {
				.iid = rte_be_to_cpu_32(paxos_hdr->inst),
				.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
				.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
				.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
				.value = *v,
			};
			paxos_message pa;
			ret = learner_receive_promise(dl->paxos_learner, &promise, &pa.u.accept);
			if (ret)
				add_paxos_message(&pa, pkt, LEARNER_PORT, ACCEPTOR_PORT, ACCEPTOR_ADDR);
			break;
		}
		case PAXOS_ACCEPTED: {
            int vsize = rte_be_to_cpu_32(paxos_hdr->value_len);
			struct paxos_value *v = paxos_value_new((char *)paxos_hdr->paxosval, vsize);
			struct paxos_accepted ack = {
				.iid = rte_be_to_cpu_32(paxos_hdr->inst),
				.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
				.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
				.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
				.value = *v,
			};
			learner_receive_accepted(dl->paxos_learner, &ack);
            struct paxos_accepted out;
            int consensus = learner_deliver_next(dl->paxos_learner, &out);
			if (consensus) {
                rte_atomic32_add(&consensus_counter, 1);
				return deliver(out.iid, out.value.paxos_value_val,
						out.value.paxos_value_len, dl);
            }
            break;
		}
		default:
			PRINT_DEBUG("No handler for %u", msgtype);
	}
	return ret;
}

static uint16_t
add_timestamps(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
        struct rte_mbuf **pkts, uint16_t nb_pkts,
        uint16_t max_pkts __rte_unused, void *user_param)
{
    uint64_t now = rte_rdtsc();
    struct dp_learner* dl = (struct dp_learner *)user_param;
    unsigned i;
    for (i = 0; i < nb_pkts; i++) {
        paxos_rx_process(pkts[i], dl);
        pkts[i]->udata64 = now;
    }
    return nb_pkts;
}

static uint16_t
calc_cycles(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
        struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
{
    uint64_t cycles = 0;
    uint64_t now = rte_rdtsc();
    unsigned i;

    for (i = 0; i < nb_pkts; i++) {
        cycles += now - pkts[i]->udata64;
    }

    rte_atomic64_add(&tot_cycles, cycles);

    return nb_pkts;
}


static inline int
port_init(uint8_t port, void* user_param)
{
    struct dp_learner *dl = (struct dp_learner *) user_param;
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
				rte_eth_dev_socket_id(port), rxconf, dl->mbuf_pool);
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
    /* Disable calculate processing latency */
	rte_eth_add_tx_callback(port, 0, calc_cycles, user_param);
	return 0;
}

static void
lcore_main(uint8_t port, struct dp_learner* dl)
{
    struct rte_mbuf *pkts_burst[BURST_SIZE];
    uint64_t prev_tsc, diff_tsc, cur_tsc;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
            BURST_TX_DRAIN_NS;

    prev_tsc = 0;

    rte_pktmbuf_alloc_bulk(dl->mbuf_tx, dl->tx_mbufs, BURST_SIZE);

	while (!force_quit) {
        cur_tsc = rte_rdtsc();
        /* TX burst queue drain */
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc)) {
            unsigned nb_tx = rte_eth_tx_buffer_flush(port, 0, dl->tx_buffer);
            if (nb_tx)
                reset_tx_mbufs(dl, nb_tx);
        }
        prev_tsc = cur_tsc;

		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, pkts_burst, BURST_SIZE);
		if (unlikely(nb_rx == 0))
			continue;
        rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_TIMER, "Received %8d packets\n", nb_rx);
	    rte_atomic32_add(&rx_counter, nb_rx);
        uint16_t idx = 0;
        for (; idx < nb_rx; idx++)
            rte_pktmbuf_free(pkts_burst[idx]);
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
	if (consensus) {
		rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "consensus reached: %d\n",
			consensus);
        rte_atomic32_add(&consensus_counter, 1);
    }
	/* this timer is automatically reloaded until we decide to stop it */
	if (force_quit)
		rte_timer_stop(tim);
}


static void
report_stat(struct rte_timer *tim, __attribute((unused)) void *arg)
{
    // int nb_tx = rte_atomic32_read(&tx_counter);
    int nb_consensus = rte_atomic32_read(&consensus_counter);
    //int nb_rx = rte_atomic32_read(&rx_counter);
    //PRINT_INFO("Throughput: tx %8d, rx %8d, drop %8d", nb_tx, nb_rx, dropped);
    uint64_t avg_cycles = rte_atomic64_read(&tot_cycles);
    if (avg_cycles > 0)
        avg_cycles /= nb_consensus;

    printf("%2d %8d, " "avg. latency = %"PRIu64" cycles, sample: %"PRIu64" cycles\n",
         at_second++, nb_consensus, avg_cycles, sample_cycle);

    rte_atomic32_set(&consensus_counter, 0);
    rte_atomic32_set(&tx_counter, 0);
    rte_atomic32_set(&rx_counter, 0);
    rte_atomic64_set(&tot_cycles, 0);

    dropped = 0;
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
	struct rte_mbuf *created_pkt = rte_pktmbuf_alloc(dl->mbuf_pool);
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
    at_second = 0;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	if (rte_log_get_global_level() == RTE_LOG_DEBUG) {
		paxos_config.verbosity = PAXOS_LOG_DEBUG;
	}

    argc -= ret;
    argv += ret;

    parse_args(argc, argv);
	//initialize learner
	struct dp_learner dp_learner = {
		.num_acceptors = learner_config.nb_acceptors,
		.nb_learners = learner_config.nb_learners,
		.learner_id = learner_config.learner_id,
        .enable_leveldb = learner_config.level_db,
        .nb_pkt_buf = 0,
	};

    dp_learner.mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
            NUM_MBUFS, MBUF_CACHE_SIZE, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (dp_learner.mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create MBUF_POOL\n");

    dp_learner.mbuf_tx = rte_pktmbuf_pool_create("MBUF_TX",
            NUM_MBUFS, MBUF_CACHE_SIZE, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (dp_learner.mbuf_tx == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create MBUF_TX\n");


	dp_learner.paxos_learner = learner_new(dp_learner.num_acceptors);
	learner_set_instance_id(dp_learner.paxos_learner, 0);

	/* load deliver_timer, every second, on a slave lcore, reloaded automatically */
	uint64_t hz = rte_get_timer_hz();

	/* Call rte_timer_manage every 10ms */
	TIMER_RESOLUTION_CYCLES = hz / 100;

    /* Init LevelDB */

    if (dp_learner.enable_leveldb) {
        dp_learner.leveldb = new_leveldb_context();
    }

	unsigned lcore_id;

#ifdef ORDER_DELIVERY
    unsigned master_core = rte_lcore_id();
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
    dp_learner.tx_buffer = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE), 0,
                rte_eth_dev_socket_id(portid));
    if (dp_learner.tx_buffer == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                (unsigned) portid);

    rte_eth_tx_buffer_init(dp_learner.tx_buffer, BURST_SIZE);
    // rte_eth_tx_buffer_set_err_callback(tx_buffer, on_sending_error, NULL);
    ret = rte_eth_tx_buffer_set_err_callback(dp_learner.tx_buffer,
                rte_eth_tx_buffer_count_callback,
                &dropped);
    if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot set error callback for "
                    "tx buffer on port %u\n", (unsigned) portid);

    /* display stats every period seconds */
    lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 1);
    rte_timer_reset(&timer, hz, PERIODICAL, lcore_id, report_stat, NULL);
    rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);

    rte_timer_subsystem_init();


    if (port_init(portid, &dp_learner) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);

	lcore_main(portid, &dp_learner);

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "free learner\n");

    /* Free LevelDB */
    if (dp_learner.enable_leveldb) {
        free_leveldb_context(dp_learner.leveldb);
    }
	learner_free(dp_learner.paxos_learner);
	return 0;
}

