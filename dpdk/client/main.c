/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
/* dpdk */
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_timer.h>
#include <rte_errno.h>
/* librte_net */
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
/* dump packet contents */
#include <rte_hexdump.h>
#include <rte_malloc.h>
/* logging */
#include <rte_log.h>
/* atomic counter */
#include <rte_atomic.h>
#include "paxos.h"
#include "rte_paxos.h"
#include "const.h"
#include "utils.h"
#include "args.h"


#define BURST_TX_DRAIN_NS 100 /* TX drain every ~100ns */

struct client {
    enum PAXOS_TEST test;
    struct rte_mempool *mbuf_pool;
    uint32_t cur_inst;
};

struct {
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;


static const struct rte_eth_conf port_conf_default = {
        .rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

static struct rte_timer timer;
static rte_atomic32_t tx_counter = RTE_ATOMIC32_INIT(0);
static rte_atomic32_t rx_counter = RTE_ATOMIC32_INIT(0);
static uint32_t dropped;

static char str[] = "Hello World";

static paxos_accept acpt = {
        .iid = 1,
        .ballot = 1,
        .value_ballot = 1,
        .aid = 0,
        .value = {sizeof(str), str},
};

static paxos_accepted accepted = {
        .iid = 1,
        .ballot = 1,
        .value_ballot = 1,
        .aid = 0,
        .value = {sizeof(str), str},
};

static void
generate_packets(struct rte_mbuf **pkts_burst, unsigned nb_tx,
    struct rte_eth_dev_tx_buffer *buffer, struct client* client)
{
    uint16_t dport = 0;
    uint32_t dstIP = 0;
    struct paxos_message pm;
    switch(client->test) {
        case PROPOSER:
            dport = PROPOSER_PORT;
            break;
        case COORDINATOR:
            dport = COORDINATOR_PORT;
            dstIP = COORDINATOR_ADDR;
            pm.type = PAXOS_ACCEPT;
            pm.u.accept = acpt;
            break;
        case ACCEPTOR:
            dport = ACCEPTOR_PORT;
            dstIP = ACCEPTOR_ADDR;
            pm.type = PAXOS_ACCEPT;
            pm.u.accept = acpt;
            break;
        case LEARNER:
            dport = LEARNER_PORT;
            dstIP = LEARNER_ADDR;
            pm.type = PAXOS_ACCEPTED;
            pm.u.accepted = accepted;
            break;
        default:
            dport = 11111;
    }

	unsigned i;
	for (i = 0; i < nb_tx; i++) {
        /* set instance number regardless of types */
        pm.u.prepare.iid = client->cur_inst;
        add_paxos_message(&pm, pkts_burst[i], 12345, dport, dstIP);
        int nb_tx = rte_eth_tx_buffer(0, 0, buffer, pkts_burst[i]);
        if (nb_tx)
            rte_atomic32_add(&tx_counter, nb_tx);

        PRINT_DEBUG("submit instance %u", client->cur_inst);
        client->cur_inst++;
    }
}

static int
send_packets(struct client *client, struct rte_eth_dev_tx_buffer *buffer, int nb_tx)
{
    struct rte_mbuf *bufs[nb_tx];
    rte_pktmbuf_alloc_bulk(client->mbuf_pool, bufs, nb_tx);
    generate_packets(bufs, nb_tx, buffer, client);
}

static int
hexdump_paxos_hdr(struct rte_mbuf *created_pkt)
{
    size_t paxos_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
                        sizeof(struct udp_hdr);
    struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt,
                                struct paxos_hdr *, paxos_offset);
    if (rte_get_log_level() == RTE_LOG_DEBUG)
        rte_hexdump(stdout, "paxos", px, sizeof(struct paxos_hdr));
}

static uint64_t prev[BURST_SIZE];

static uint16_t __attribute((unused))
check_return(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
        struct rte_mbuf **pkts, uint16_t nb_pkts,
        uint16_t max_pkts __rte_unused, void *user_param __rte_unused)
{
    uint64_t now = rte_rdtsc();
    unsigned i;

    for (i = 0; i < nb_pkts; i++) {
        pkts[i]->udata64 = now;
        hexdump_paxos_hdr(pkts[i]);
    }
    return nb_pkts;
};

static uint16_t __attribute((unused))
add_timestamp(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
    __attribute((unused)) struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
{
    unsigned i;
    for (i = 0; i < nb_pkts; i++)
        prev[i] = rte_rdtsc();
    return nb_pkts;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;
    uint16_t q;
    if (port >= rte_eth_dev_count())
            return -1;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf *txconf;
    struct rte_eth_rxconf *rxconf;
    rte_eth_dev_info_get(port, &dev_info);

    rxconf = &dev_info.default_rxconf;
    txconf = &dev_info.default_txconf;

    txconf->txq_flags &= PKT_TX_IPV4;
    txconf->txq_flags &= PKT_TX_UDP_CKSUM;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
            return retval;
    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
            retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                            rte_eth_dev_socket_id(port), rxconf, mbuf_pool);
            if (retval < 0)
                    return retval;
    }
    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
            retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                            rte_eth_dev_socket_id(port), txconf);
            if (retval < 0)
                return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
            return retval;
    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);

    rte_eth_add_rx_callback(port, 0, check_return, NULL);
    rte_eth_add_tx_callback(port, 0, calc_latency, NULL);

    return 0;
}
/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void
lcore_main(uint8_t port, struct client* client, struct rte_eth_dev_tx_buffer *buffer)
{
    struct rte_mbuf *pkts_burst[BURST_SIZE];
    uint64_t prev_tsc, diff_tsc, cur_tsc;
    unsigned i, nb_rx;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + NS_PER_S - 1) / NS_PER_S *
            BURST_TX_DRAIN_NS;

    prev_tsc = 0;


    send_packets(client, buffer, BURST_SIZE);

    /* Run until the application is quit or killed. */
    while (!force_quit) {

        cur_tsc = rte_rdtsc();
        /* TX burst queue drain */
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc)) {
            unsigned nb_tx = rte_eth_tx_buffer_flush(port, 0, buffer);
            rte_atomic32_add(&tx_counter, nb_tx);
        }

        prev_tsc = cur_tsc;

        nb_rx = rte_eth_rx_burst(port, 0, pkts_burst, BURST_SIZE);
        rte_atomic32_add(&rx_counter, nb_rx);
        /* Free packets. */
        for (i = 0; i < nb_rx; i++)
            rte_pktmbuf_free(pkts_burst[i]);
        if (nb_rx)
            send_packets(client, buffer, nb_rx);
    }
}

static void
report_stat(struct rte_timer *tim, __attribute((unused)) void *arg)
{
    PRINT_DEBUG("%s on core %d", __func__, rte_lcore_id());
    int nb_tx = rte_atomic32_read(&tx_counter);
    int nb_rx = rte_atomic32_read(&rx_counter);
    PRINT_INFO("Throughput: tx %8d, rx %8d, drop %8d", nb_tx, nb_rx, dropped);
    rte_atomic32_set(&tx_counter, 0);
    rte_atomic32_set(&rx_counter, 0);
    dropped = 0;
    if (force_quit)
        rte_timer_stop(tim);
}
/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
    struct rte_eth_dev_info dev_info;
    unsigned lcore_id;
    uint8_t portid = 0;
	force_quit = false;
    struct rte_eth_dev_tx_buffer *tx_buffer;

    /* Learner's instance starts at 1 */
    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    parse_args(argc, argv);

    struct client client;
    client.test = client_config.test;
    client.cur_inst = 1;

    rte_timer_init(&timer);
    uint64_t hz = rte_get_timer_hz();

    rte_eth_dev_info_get(portid, &dev_info);
    /* increase call frequency to rte_timer_manage() to increase the precision */
    TIMER_RESOLUTION_CYCLES = hz / 100;
    PRINT_INFO("1 cycle is %3.2f ns", 1E9 / (double)hz);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

    /* Creates a new mempool in memory to hold the mbufs. */
    client.mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
            MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (client.mbuf_pool == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    tx_buffer = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE), 0,
                rte_eth_dev_socket_id(portid));
    if (tx_buffer == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                (unsigned) portid);

    rte_eth_tx_buffer_init(tx_buffer, BURST_SIZE);
    ret = rte_eth_tx_buffer_set_err_callback(tx_buffer,
                rte_eth_tx_buffer_count_callback,
                &dropped);

    /* Initialize port 0. */
    if (port_init(portid, client.mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);

    /* display stats every period seconds */
    lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 1);
    rte_timer_reset(&timer, hz, PERIODICAL, lcore_id, report_stat, NULL);
    rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);

	rte_timer_subsystem_init();

    /* Call lcore_main on the master core only. */
    lcore_main(portid, &client, tx_buffer);
    return 0;
}
