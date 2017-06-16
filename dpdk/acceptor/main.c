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
#include <rte_errno.h>

#include "utils.h"
#include "const.h"
#include "rte_paxos.h"
#include "args.h"

#define USE_TX_BUFFER

#define BURST_TX_DRAIN_NS 100 /* TX drain every ~100ns */

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};

extern struct {
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;

static const struct ether_addr ether_multicast = {
	.addr_bytes= { 0x01, 0x1b, 0x19, 0x0, 0x0, 0x0 }
};

#define ETHER_ADDR_FOR_IPV4_MCAST(x)    \
        (rte_cpu_to_be_64(0x01005e000000ULL | ((x) & 0x7fffff)) >> 16)

struct rte_mempool *mbuf_pool;

#ifdef USE_TX_BUFFER
    struct rte_eth_dev_tx_buffer *tx_buffer;
#endif

static struct rte_timer timer;
static rte_atomic32_t tx_counter = RTE_ATOMIC32_INIT(0);
static rte_atomic32_t rx_counter = RTE_ATOMIC32_INIT(0);
static uint32_t dropped;

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
	struct udp_hdr *udp_hdr;
	struct paxos_hdr *paxos_hdr;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

    int l2_len = sizeof(struct ether_hdr);
    int l3_len = sizeof(struct ipv4_hdr);
    int l4_len = sizeof(struct udp_hdr);

    struct ipv4_hdr *iph = (struct ipv4_hdr *) ((char *)phdr + l2_len);
    if (rte_log_get_global_level() == RTE_LOG_DEBUG)
        rte_hexdump(stdout, "ip", iph, sizeof(struct ipv4_hdr));

    if (iph->next_proto_id != IPPROTO_UDP)
        return -1;

    udp_hdr = (struct udp_hdr *)((char *)iph + l3_len);

    if (rte_log_get_global_level() == RTE_LOG_DEBUG)
        rte_hexdump(stdout, "udp", udp_hdr, sizeof(struct udp_hdr));

    if (udp_hdr->dst_port != rte_cpu_to_be_16(ACCEPTOR_PORT) &&
            (pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
        return -1;

    paxos_hdr = (struct paxos_hdr *)((char *)udp_hdr + l4_len);

	if (rte_log_get_global_level() == RTE_LOG_DEBUG)
        rte_hexdump(stdout, "paxos", paxos_hdr, sizeof(struct paxos_hdr));

    int value_len = rte_be_to_cpu_32(paxos_hdr->value_len);
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
			.value = {0, NULL}};
			//dump_prepare_message(&prepare);
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
			.aid = rte_be_to_cpu_16(paxos_hdr->acptid) };
            accept.value.paxos_value_len = value_len;
            accept.value.paxos_value_val = (char*)&paxos_hdr->paxosval;
			ret = acceptor_receive_accept(acceptor, &accept, &out);
			break;
		}
		default:
			rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Unrecognized Paxos message\n");
	}

	// Acceptor has cast a vote
    size_t data_len = sizeof(struct paxos_hdr);
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
				paxos_hdr->value_len = rte_cpu_to_be_32(out.u.promise.value.paxos_value_len);
                data_len += out.u.promise.value.paxos_value_len;
				if (out.u.promise.value.paxos_value_len != 0)
					rte_memcpy(paxos_hdr->paxosval, out.u.promise.value.paxos_value_val,
							out.u.promise.value.paxos_value_len);
				break;
			}
			case PAXOS_ACCEPTED:
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Accepted Paxos message\n");
				paxos_hdr->acptid = rte_cpu_to_be_16(out.u.accepted.aid);
                data_len += out.u.accepted.value.paxos_value_len;
				break;
			case PAXOS_PREEMPTED:
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Preempted Paxos message\n");
				paxos_hdr->rnd = rte_cpu_to_be_16(out.u.preempted.ballot);
				paxos_hdr->vrnd = rte_cpu_to_be_16(out.u.preempted.value_ballot);
				paxos_hdr->acptid = rte_cpu_to_be_16(out.u.preempted.aid);
                data_len += out.u.preempted.value.paxos_value_len;
				break;
			default:
				rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Unrecognized Paxos message\n");
		}
	} else {
		rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
					"Acceptor rejected Paxos message\n");
		return -1;
	}
	union {
        uint64_t as_int;
        struct ether_addr as_addr;
    } dst_eth_addr;
	dst_eth_addr.as_int = ETHER_ADDR_FOR_IPV4_MCAST(LEARNER_ADDR);
	ether_addr_copy(&dst_eth_addr.as_addr, &phdr->d_addr);

    iph->dst_addr = rte_cpu_to_be_32(LEARNER_ADDR);
    iph->hdr_checksum = 0;
	pkt->l2_len = l2_len;
	pkt->l3_len = l3_len;
    // pkt->l4_len = rte_be_to_cpu_16(udp_hdr->dgram_len);
	pkt->l4_len = rte_be_to_cpu_16(l4_len + data_len);
	pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
	udp_hdr->src_port = rte_cpu_to_be_16(ACCEPTOR_PORT);
	udp_hdr->dst_port = rte_cpu_to_be_16(LEARNER_PORT);
	udp_hdr->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, pkt->ol_flags);
	return 0;

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
		if (paxos_rx_process(pkts[i], acceptor) < 0) {
			rte_pktmbuf_free(pkts[i]);
        }
        else {
#ifdef USE_TX_BUFFER
            int nb_tx = rte_eth_tx_buffer(0, 0, tx_buffer, pkts[i]);
            if (nb_tx)
                rte_atomic32_add(&tx_counter, nb_tx);
#else
            rte_eth_tx_burst(0, 0, &pkt, 1);
            rte_atomic32_add(&tx_counter, 1);
#endif
        }
	}

	return nb_pkts;
}


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

static void __attribute((unused))
on_sending_error(struct rte_mbuf **unsent, uint16_t count,
	__attribute((unused)) void *userdata)
{
    PRINT_DEBUG("tx_buffer error: %s, %d packets", rte_strerror(rte_errno), count);
    int i;
    for (i = 0; i < count; i++)
	    rte_pktmbuf_free(unsent[i]);
}

static void
report_stat(struct rte_timer *tim, __attribute((unused)) void *arg)
{
    PRINT_DEBUG("%s on core %d", __func__, rte_lcore_id());
    int nb_tx = rte_atomic32_read(&tx_counter);
    int nb_rx = rte_atomic32_read(&rx_counter);
    uint64_t avg_cycles = 0;
    if (latency_numbers.total_cycles > 0)
        avg_cycles = latency_numbers.total_cycles / latency_numbers.total_pkts;
    PRINT_INFO("Throughput: tx %8d, rx %8d, drop %8d, avg_cycles: %"PRIu64"", nb_tx, nb_rx, dropped, avg_cycles);
    rte_atomic32_set(&tx_counter, 0);
    rte_atomic32_set(&rx_counter, 0);
    latency_numbers.total_cycles = 0;
    latency_numbers.total_pkts = 0;
    dropped = 0;
    if (force_quit)
        rte_timer_stop(tim);
}


static void
lcore_main(uint8_t port)
{
    struct rte_mbuf *pkts_burst[BURST_SIZE];
    unsigned nb_rx;

#ifdef USE_TX_BUFFER
    uint64_t prev_tsc, diff_tsc, cur_tsc;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + NS_PER_S - 1) / NS_PER_S *
            BURST_TX_DRAIN_NS;
    prev_tsc = 0;
#endif

    /* Run until the application is quit or killed. */
    while (!force_quit) {
#ifdef USE_TX_BUFFER
        /* TX burst queue drain */
        cur_tsc = rte_rdtsc();
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc)) {
            unsigned nb_tx = rte_eth_tx_buffer_flush(port, 0, tx_buffer);
            rte_atomic32_add(&tx_counter, nb_tx);
        }
        prev_tsc = cur_tsc;
#endif
		nb_rx = rte_eth_rx_burst(port, 0, pkts_burst, BURST_SIZE);
		if (unlikely(nb_rx == 0))
			continue;
	    rte_atomic32_add(&rx_counter, nb_rx);

	}
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

    argc -= ret;
    argv += ret;

    parse_args(argc, argv);

	rte_timer_init(&timer);
    uint64_t hz = rte_get_timer_hz();
    PRINT_INFO("1 cycle is %3.2f ns", 1E9 / (double)hz);


	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");

#ifdef USE_TX_BUFFER
    tx_buffer = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE), 0,
                rte_eth_dev_socket_id(portid));
    if (tx_buffer == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                (unsigned) portid);

    rte_eth_tx_buffer_init(tx_buffer, BURST_SIZE);
    // rte_eth_tx_buffer_set_err_callback(tx_buffer, on_sending_error, NULL);
    ret = rte_eth_tx_buffer_set_err_callback(tx_buffer,
                rte_eth_tx_buffer_count_callback,
                &dropped);
    if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot set error callback for "
                    "tx buffer on port %u\n", (unsigned) portid);

#endif

	//initialize acceptor
	struct acceptor *acceptor = acceptor_new(acceptor_config.acceptor_id);

	if (port_init(portid, mbuf_pool, acceptor) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);

    /* display stats every period seconds */
    int lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 1);
    rte_timer_reset(&timer, hz, PERIODICAL, lcore_id, report_stat, NULL);
    rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);

    rte_timer_subsystem_init();

	lcore_main(portid);

	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER1, "free acceptor\n");
	acceptor_free(acceptor);
	return 0;
}

