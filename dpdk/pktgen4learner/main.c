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
#include "message.h"

#define NS_PER_S 1000000000

struct client {
    enum PAXOS_TEST test;
    struct rte_mempool *mbuf_pool;
    uint32_t cur_inst;
    enum Operation op;
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

static double latency;
static struct sockaddr_in client_addr = {
    .sin_family = AF_INET,
    .sin_port   = 3490,
};

static void
generate_packet(struct client* client)
{
    uint16_t dport = 0;
    uint32_t dstIP = 0;
    struct paxos_message pm;

    struct command cmd;
    clock_gettime(CLOCK_REALTIME, &cmd.ts);
    cmd.op = client->op;
    memset(cmd.content, 'k', 15);
    cmd.content[15] = '\0';
    memset(cmd.content+16, 'v', 15);
    cmd.content[31] = '\0';
    struct client_request *req = create_client_request((char*)&cmd, sizeof(cmd));
    req->cliaddr = client_addr;
    paxos_accepted phase2b = {
            .iid = 1,
            .ballot = 1,
            .value_ballot = 1,
            .aid = 0,
            .value ={message_length(req), (char*)req},
    };

    dport = LEARNER_PORT;
    dstIP = LEARNER_ADDR;
    pm.type = PAXOS_ACCEPTED;
    pm.u.accepted = phase2b;

    /* set instance number regardless of types */
    pm.u.prepare.iid = client->cur_inst++;
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(client->mbuf_pool);

    add_paxos_message(&pm, pkt, 12345, dport, dstIP);
    int nb_tx = rte_eth_tx_burst(0, 0, &pkt, 1);
    if (nb_tx)
        rte_atomic32_add(&tx_counter, nb_tx);
    free(req);
}

static int
timespec_diff(struct timespec *result, struct timespec *end,struct timespec *start)
{
    if (end->tv_nsec < start->tv_nsec) {
        result->tv_nsec =  NS_PER_S + end->tv_nsec - start->tv_nsec;
        result->tv_sec = end->tv_sec - start->tv_sec - 1;
    } else {
        result->tv_nsec = end->tv_nsec - start->tv_nsec;
        result->tv_sec = end->tv_sec - start->tv_sec;
    }
  /* Return 1 if result is negative. */
  return end->tv_sec < start->tv_sec;
}

static void
compute_latency(struct rte_mbuf *pkt)
{
    int l2_len = sizeof(struct ether_hdr);
    struct ipv4_hdr *iph = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, l2_len);

    if (iph->next_proto_id != IPPROTO_UDP)
        return;

    int l3_len = sizeof(struct ipv4_hdr);
    struct udp_hdr *udp_hdr = (struct udp_hdr *)((char *)iph + l3_len);

    int l4_len = sizeof(struct udp_hdr);
    struct command *cmd = (struct command *)((char*)udp_hdr + l4_len);

    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    struct timespec result;
    if ( timespec_diff(&result, &end_time, &cmd->ts) < 1) {
        latency += result.tv_sec*NS_PER_S + result.tv_nsec;
        rte_atomic32_add(&rx_counter, 1);
    }
}

static uint16_t
on_receive(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
        struct rte_mbuf **pkts, uint16_t nb_pkts,
        uint16_t max_pkts __rte_unused, void *user_param __rte_unused)
{
    struct client *client = (struct client *)user_param;
    unsigned i;

    for (i = 0; i < nb_pkts; i++) {
        compute_latency(pkts[i]);
        generate_packet(client);
    }
    return nb_pkts;
};

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct client *client)
{
    struct rte_mempool *mbuf_pool = client->mbuf_pool;
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

    rte_eth_add_rx_callback(port, 0, on_receive, client);
    // rte_eth_add_tx_callback(port, 0, calc_latency, NULL);

    return 0;
}
/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void
lcore_main(uint8_t port, struct client* client, int outstanding)
{
    int i, nb_rx;

    for (i = 0; i < outstanding; i++) {
        generate_packet(client);
    }

    struct rte_mbuf *pkts_burst[BURST_SIZE];

    /* Run until the application is quit or killed. */
    while (!force_quit) {
        nb_rx = rte_eth_rx_burst(port, 0, pkts_burst, BURST_SIZE);
        /* Free packets. */
        for (i = 0; i < nb_rx; i++)
            rte_pktmbuf_free(pkts_burst[i]);
    }
}

static void
report_stat(struct rte_timer *tim, __attribute((unused)) void *arg)
{
    PRINT_DEBUG("%s on core %d", __func__, rte_lcore_id());
    int nb_tx = rte_atomic32_read(&tx_counter);
    int nb_rx = rte_atomic32_read(&rx_counter);
    double avg_latency = 0.0;
    if (nb_rx) {
        avg_latency = latency / nb_rx / NS_PER_S;
    }

    PRINT_INFO("Throughput: tx %8d, rx %8d avg_latency %.09f", nb_tx, nb_rx, avg_latency);
    rte_atomic32_set(&tx_counter, 0);
    rte_atomic32_set(&rx_counter, 0);
    latency = 0.0;
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
    uint8_t portid = 0;
	force_quit = false;

    inet_aton("192.168.4.97", &client_addr.sin_addr);
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
    client.op = client_config.write_op;

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

    /* Initialize port 0. */
    if (port_init(portid, &client) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);

    unsigned lcore_id = rte_get_next_lcore(rte_lcore_id(), 0, 1);
    rte_timer_reset(&timer, hz, PERIODICAL, lcore_id, report_stat, NULL);
    rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);

	rte_timer_subsystem_init();

    /* Call lcore_main on the master core only. */
    lcore_main(portid, &client, client_config.outstanding);
    return 0;
}
