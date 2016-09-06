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
/* dpdk */
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_timer.h>
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

static uint64_t CYCLES_PER_SECOND;

static uint32_t cur_inst;

static const struct rte_eth_conf port_conf_default = {
        .rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

static struct rte_timer timer;

static rte_atomic32_t counter = RTE_ATOMIC32_INIT(0);

#define MEASUREMENT_PERIOD 5
static uint32_t total_tp;
static uint32_t at_second;

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
	if (ethertype == ETHER_TYPE_IPv4)
		return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
	else /* assume ethertype == ETHER_TYPE_IPv6 */
		return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

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
            sizeof(struct udp_hdr) + sizeof(struct paxos_message));
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

static void
add_paxos_message(struct paxos_message *pm, struct rte_mbuf *created_pkt)
{
    uint8_t port_id = 0;
    // struct rte_mbuf *created_pkt = rte_pktmbuf_alloc(mbuf_pool);
    created_pkt->l2_len = sizeof(struct ether_hdr);
    created_pkt->l3_len = sizeof(struct ipv4_hdr);
    created_pkt->l4_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
    uint64_t ol_flags = craft_new_packet(&created_pkt, IPv4(192,168,4,95),
                        IPv4(239,3,29,73), PROPOSER_PORT, ACCEPTOR_PORT,
                        sizeof(struct paxos_message), port_id);
    size_t udp_offset = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
    size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);

    struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt, struct paxos_hdr *, paxos_offset);
    px->msgtype = rte_cpu_to_be_16(pm->type);
    px->inst = rte_cpu_to_be_32(pm->u.accept.iid);
    px->rnd = rte_cpu_to_be_16(pm->u.accept.ballot);
    px->vrnd = rte_cpu_to_be_16(pm->u.accept.value_ballot);
    px->acptid = 0;
    px->value_len = rte_cpu_to_be_16(pm->u.accept.value.paxos_value_len);
    rte_memcpy(px->paxosval, pm->u.accept.value.paxos_value_val, pm->u.accept.value.paxos_value_len);
    created_pkt->ol_flags = ol_flags;
}

static void __attribute((unused))
send_batch(struct rte_mbuf **mbufs, int count, int port_id)
{
    uint16_t nb_tx;
    do {
        nb_tx = rte_eth_tx_burst(port_id, 0, mbufs, count);
    	rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8, "Send %d messages\n", nb_tx);
    } while (nb_tx == count);
}


static int
generate_packets(__attribute__((unused)) void *arg)
{
        int ret;
        unsigned lcore_id;
        struct rte_mempool *mbuf_pool = (struct rte_mempool*) arg;
        struct paxos_accept accept = {
            .iid = (cur_inst++),
            .ballot = (1),
            .value_ballot = (1),
            .aid = 0,
            .value = {0, NULL},
        };
        struct paxos_message pm;
        pm.type = (PAXOS_PREPARE);
        pm.u.accept = accept;
        lcore_id = rte_lcore_id();
        printf("%s on core %u\n", __func__, lcore_id);
        struct rte_mbuf *bufs[BURST_SIZE];
        ret = rte_pktmbuf_alloc_bulk(mbuf_pool, bufs, BURST_SIZE);
	while (1) {
        	unsigned i;
        	for (i = 0; i < BURST_SIZE; i++)
        	    add_paxos_message(&pm, bufs[i]);
        	send_batch(bufs, BURST_SIZE, 0);
	}
        return ret;
}

/* basicfwd.c: Basic DPDK skeleton forwarding example. */
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
        /* Configure the Ethernet device. */
        retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
        if (retval != 0)
                return retval;
        /* Allocate and set up 1 RX queue per Ethernet port. */
        for (q = 0; q < rx_rings; q++) {
                retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                                rte_eth_dev_socket_id(port), NULL, mbuf_pool);
                if (retval < 0)
                        return retval;
        }
        /* Allocate and set up 1 TX queue per Ethernet port. */
        for (q = 0; q < tx_rings; q++) {
                retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                                rte_eth_dev_socket_id(port), NULL);
                if (retval < 0)
                        return retval;
        }
        /* Start the Ethernet port. */
        retval = rte_eth_dev_start(port);
        if (retval < 0)
                return retval;
        /* Enable RX in promiscuous mode for the Ethernet device. */
        rte_eth_promiscuous_enable(port);
        return 0;
}
/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void
lcore_main(void)
{
        uint8_t port = 0;
        /*
         * Check that the port is on the same NUMA node as the polling thread
         * for best performance.
         */
        if (rte_eth_dev_socket_id(port) > 0 &&
                                rte_eth_dev_socket_id(port) !=
                                                (int)rte_socket_id())
                printf("WARNING, port %u is on remote NUMA node to "
                                        "polling thread.\n\tPerformance will "
                                        "not be optimal.\n", port);
        printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
                        rte_lcore_id());
        /* Run until the application is quit or killed. */
        for (;;) {
		if (force_quit)
			break;
                 struct rte_mbuf *bufs[BURST_SIZE];
                 const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                                 bufs, BURST_SIZE);
                rte_atomic32_add(&counter, nb_rx);
                 if (unlikely(nb_rx == 0))
                         continue;
                 /* Free packets. */
                 uint16_t buf;
                 for (buf = 0; buf < nb_rx; buf++)
                         rte_pktmbuf_free(bufs[buf]);
        }
}

static void
report_stat(struct rte_timer *tim, __attribute((unused)) void *arg)
{
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_TIMER,
            "%s on core %d\n", __func__, rte_lcore_id());
    int count = rte_atomic32_read(&counter);
    total_tp += count;
    // rte_log(RTE_LOG_INFO, RTE_LOGTYPE_TIMER,
    //             "%8"PRIu32 "\n", count);
    rte_atomic32_set(&counter, 0);
    if (at_second == MEASUREMENT_PERIOD) {
        rte_log(RTE_LOG_INFO, RTE_LOGTYPE_TIMER,
                    "%8d \n", total_tp / MEASUREMENT_PERIOD);
        total_tp = 0;
        at_second = 0;
    } else
        at_second++;

	if (force_quit)
		rte_timer_stop(tim);
}

static __attribute((noreturn)) int
lcore_mainloop(__attribute((unused)) void *arg)
{
	uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
	unsigned lcore_id;

	lcore_id = rte_lcore_id();

	rte_log(RTE_LOG_INFO, RTE_LOGTYPE_TIMER,
		"Starting mainloop on core %u\n", lcore_id);
	while(1) {
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;
		if (diff_tsc > CYCLES_PER_SECOND) {
			rte_timer_manage();
			prev_tsc = cur_tsc;
		}
	}
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

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
        struct rte_mempool *mbuf_pool;
        unsigned master_core, lcore_id;
        uint8_t portid = 0;
    	force_quit = false;
        cur_inst = 0;
        /* Initialize the Environment Abstraction Layer (EAL). */
        int ret = rte_eal_init(argc, argv);
        if (ret < 0)
                rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	rte_timer_init(&timer);
	CYCLES_PER_SECOND = rte_get_timer_hz();
    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_USER8,
            "1 cycle is %3.2f ns\n", 1E9 / (double)CYCLES_PER_SECOND);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

        /* Creates a new mempool in memory to hold the mbufs. */
        mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
        if (mbuf_pool == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
        /* Initialize port 0. */
        if (port_init(portid, mbuf_pool) != 0)
                rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);

        /* master core */
        master_core = rte_lcore_id();
        /* slave core */
        lcore_id = rte_get_next_lcore(master_core, 0, 1);
	rte_timer_reset(&timer, CYCLES_PER_SECOND, PERIODICAL, lcore_id, report_stat, NULL);
	rte_timer_subsystem_init();
	rte_eal_remote_launch(lcore_mainloop, NULL, lcore_id);

        /* slave core */
        lcore_id = rte_get_next_lcore(lcore_id, 0, 1);
	rte_eal_remote_launch(generate_packets, mbuf_pool, lcore_id);


        /* Call lcore_main on the master core only. */
        lcore_main();
        return 0;
}
