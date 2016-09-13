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

#define BURST_TX_DRAIN_NS 10 /* TX drain every ~100ns */

/*
 * Construct Ethernet multicast address from IPv4 multicast address.
 * Citing RFC 1112, section 6.4:
 * "An IP host group address is mapped to an Ethernet multicast address
 * by placing the low-order 23-bits of the IP address into the low-order
 * 23 bits of the Ethernet multicast address 01-00-5E-00-00-00 (hex)."
 */
#define ETHER_ADDR_FOR_IPV4_MCAST(x)    \
        (rte_cpu_to_be_64(0x01005e000000ULL | ((x) & 0x7fffff)) >> 16)

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};


struct rte_mempool *mbuf_pool;

struct coordinator {
	uint32_t cur_inst; /* Paxos instance */
    struct rte_eth_dev_tx_buffer *tx_buffer;
};

static int
paxos_rx_process(struct rte_mbuf *pkt, struct coordinator *cord)
{
	uint8_t l4_proto = 0;
	union tunnel_offload_info info = { .data = 0 };
	struct ipv4_hdr *iph;
	struct udp_hdr *udp_hdr;
	struct paxos_hdr *paxos_hdr;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	union {
        uint64_t as_int;
        struct ether_addr as_addr;
    } dst_eth_addr;
	parse_ethernet(phdr, &info, &l4_proto);

	if (l4_proto != IPPROTO_UDP)
		return -1;

	iph = (struct ipv4_hdr *) ((char *)phdr + info.outer_l2_len);
	udp_hdr = (struct udp_hdr *)((char *)phdr +
			info.outer_l2_len + info.outer_l3_len);

	if (udp_hdr->dst_port != rte_cpu_to_be_16(COORDINATOR_PORT) &&
			(pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
		return -1;

	paxos_hdr = (struct paxos_hdr *)((char *)udp_hdr + sizeof(struct udp_hdr));

	if (rte_get_log_level() == RTE_LOG_DEBUG) {
		rte_hexdump(stdout, "udp", udp_hdr, sizeof(struct udp_hdr));
		rte_hexdump(stdout, "paxos", paxos_hdr, sizeof(struct paxos_hdr));
		print_paxos_hdr(paxos_hdr);
	}
	dst_eth_addr.as_int = ETHER_ADDR_FOR_IPV4_MCAST(ACCEPTOR_ADDR);
	ether_addr_copy(&dst_eth_addr.as_addr, &phdr->d_addr);
    iph->dst_addr = rte_cpu_to_be_32(ACCEPTOR_ADDR);
    iph->hdr_checksum = 0;
	paxos_hdr->inst = rte_cpu_to_be_32(cord->cur_inst++);
	pkt->l2_len = info.outer_l2_len;
	pkt->l3_len = info.outer_l3_len;
	pkt->l4_len = rte_be_to_cpu_16(udp_hdr->dgram_len);
	pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
	udp_hdr->dst_port = rte_cpu_to_be_16(ACCEPTOR_PORT);
	udp_hdr->dgram_cksum = get_psd_sum(iph, ETHER_TYPE_IPv4, pkt->ol_flags);
    rte_eth_tx_buffer(0, 0, cord->tx_buffer, pkt);
	return 0;
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
		if(paxos_rx_process(pkts[i], cord) < 0)
			rte_pktmbuf_free(pkts[i]);
	}
	return nb_pkts;
}


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
lcore_main(uint8_t port, struct coordinator* cord)
{
    struct rte_mbuf *pkts_burst[BURST_SIZE];
    uint64_t prev_tsc, diff_tsc, cur_tsc;
    unsigned nb_rx;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + NS_PER_S - 1) / NS_PER_S *
            BURST_TX_DRAIN_NS;

    prev_tsc = 0;

    /* Run until the application is quit or killed. */
    while (!force_quit) {

        cur_tsc = rte_rdtsc();
        /* TX burst queue drain */
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc)) {
            rte_eth_tx_buffer_flush(port, 0, cord->tx_buffer);
        }

        prev_tsc = cur_tsc;
		nb_rx = rte_eth_rx_burst(port, 0, pkts_burst, BURST_SIZE);
		if (unlikely(nb_rx == 0))
			continue;
	}
}


int
main(int argc, char *argv[])
{
	uint8_t portid = 0;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	struct coordinator cord = { .cur_inst = 1 };
	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");


	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");

    cord.tx_buffer = rte_zmalloc_socket("tx_buffer",
                RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE), 0,
                rte_eth_dev_socket_id(portid));
    if (cord.tx_buffer == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                (unsigned) portid);

    rte_eth_tx_buffer_init(cord.tx_buffer, BURST_SIZE);


	if (port_init(portid, mbuf_pool, &cord) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);

	lcore_main(portid, &cord);

	return 0;
}

