#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
/* get cores */
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_timer.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_arp.h>
#include <rte_icmp.h>
#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <endian.h>
/* dump packet contents */
#include <rte_hexdump.h>
#include <rte_malloc.h>
/* logging */
#include <rte_log.h>
#include "utils.h"
#include "const.h"

extern struct {
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;


#define BURST_TX_DRAIN_NS 100 /* TX drain every ~100ns */

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {.max_rx_pkt_len = ETHER_MAX_LEN}
};
/*
static const struct ether_addr ether_acceptor_macaddr = {
    .addr_bytes= { 0x08, 0x00, 0x27, 0x85, 0xad, 0xcb }
};
*/
/*
 * Construct Ethernet multicast address from IPv4 multicast address.
 * Citing RFC 1112, section 6.4:
 * "An IP host group address is mapped to an Ethernet multicast address
 * by placing the low-order 23-bits of the IP address into the low-order
 * 23 bits of the Ethernet multicast address 01-00-5E-00-00-00 (hex)."
 */
#define ETHER_ADDR_FOR_IPV4_MCAST(x)    \
        (rte_cpu_to_be_64(0x01005e000000ULL | ((x) & 0x7fffff)) >> 16)

struct rte_mempool *mbuf_pool;
struct coordinator
{
	uint32_t cur_inst;
	struct rte_eth_dev_tx_buffer *tx_buffer;
};

static struct rte_timer timer;
static rte_atomic32_t tx_counter = RTE_ATOMIC32_INIT(0);
static rte_atomic32_t rx_counter = RTE_ATOMIC32_INIT(0);

static void
report_stat(struct rte_timer *tim, __attribute((unused)) void *arg)
{
	int nb_tx = rte_atomic32_read(&tx_counter);
	int nb_rx = rte_atomic32_read(&rx_counter);
    uint64_t avg_cycles = 0;
    if (latency_numbers.total_cycles > 0)
        avg_cycles = latency_numbers.total_cycles / latency_numbers.total_pkts;
    PRINT_INFO("Throughput: tx %8d, rx %8d, avg_cycles: %"PRIu64"", nb_tx, nb_rx, avg_cycles);
    latency_numbers.total_cycles = 0;
    latency_numbers.total_pkts = 0;
	rte_atomic32_set(&rx_counter, 0);
	rte_atomic32_set(&tx_counter, 0);
	if(force_quit)
		rte_timer_stop(tim);
}
static int
paxos_rx_process(uint8_t port, struct rte_mbuf *pkt, struct coordinator *coord)
{
	struct ipv4_hdr *ip_h;
	struct udp_hdr *udp_h;
	struct paxos_hdr *paxos_h;
	struct ether_hdr *eth_h;
	struct arp_hdr *arp_h;
	struct icmp_hdr *icmp_h;

	uint32_t ip_addr;
	uint16_t eth_type;
	uint16_t arp_op;
	uint16_t arp_pro;
	int l2_len = sizeof(struct ether_hdr);
	int l3_len = sizeof(struct ipv4_hdr);
	struct ether_addr eth_addr;

	union {
        uint64_t as_int;
        struct ether_addr as_addr;
    } dst_eth_addr;

	eth_h = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_h->ether_type);
	if (eth_type == ETHER_TYPE_ARP)	{

		arp_h = (struct arp_hdr *) ((char *)eth_h + l2_len);
		arp_op = rte_be_to_cpu_16(arp_h->arp_op);
		arp_pro = rte_be_to_cpu_16(arp_h->arp_pro);

		if ((rte_be_to_cpu_16(arp_h->arp_hrd) !=ARP_HRD_ETHER) ||(arp_pro != ETHER_TYPE_IPv4) ||
			    (arp_h->arp_hln != 6) || (arp_h->arp_pln != 4))
			return -1;

		if (arp_op != ARP_OP_REQUEST)
			return -1;

		/*ARP reply*/
		ether_addr_copy(&eth_h->s_addr, &eth_h->d_addr);
		rte_eth_macaddr_get(port, &eth_h->s_addr);
		arp_h->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);

		ip_addr = arp_h->arp_data.arp_sip;
		arp_h->arp_data.arp_sip = arp_h->arp_data.arp_tip;
		rte_eth_macaddr_get(port, &eth_addr);

		//set arp_sip and arp_sha
		ether_addr_copy(&eth_addr, &arp_h->arp_data.arp_sha);
		arp_h->arp_data.arp_tip = ip_addr;
		pkt->l2_len = l2_len;
		pkt->l3_len = l3_len;
		unsigned nb_tx = rte_eth_tx_buffer(0, 0, coord->tx_buffer, pkt);
		if(nb_tx)
			rte_atomic32_add(&tx_counter, nb_tx);
		return 0;
	}
	if(eth_type != ETHER_TYPE_IPv4)
		return -1;

	ip_h = (struct ipv4_hdr*) ((char *)eth_h + l2_len);
	if(rte_log_get_global_level() == RTE_LOG_DEBUG)
		rte_hexdump(stdout, "ip", ip_h, sizeof(struct ipv4_hdr));

	if(ip_h->next_proto_id == IPPROTO_ICMP)
	{
		icmp_h = (struct icmp_hdr *) ((char *)ip_h + l3_len);
		ether_addr_copy(&eth_h->s_addr, &eth_addr);
		ether_addr_copy(&eth_h->d_addr, &eth_h->s_addr);
		ether_addr_copy(&eth_addr, &eth_h->d_addr);
		ip_addr = ip_h->src_addr;
		ip_h->src_addr = ip_h->dst_addr;
		ip_h->dst_addr = ip_addr;
		icmp_h->icmp_type = IP_ICMP_ECHO_REPLY;
		icmp_h->icmp_cksum = 0;
		pkt->l2_len = l2_len;
		pkt->l3_len = l3_len;
		unsigned nb_tx = rte_eth_tx_buffer(0, 0, coord->tx_buffer, pkt);
		if(nb_tx)
			rte_atomic32_add(&tx_counter, nb_tx);
		return 0;
	}
	else if (ip_h->next_proto_id == IPPROTO_UDP)
	{

		udp_h = (struct udp_hdr *)((char *)ip_h + l3_len);

		if(rte_log_get_global_level() == RTE_LOG_DEBUG)
			rte_hexdump(stdout, "udp", udp_h, sizeof(struct udp_hdr));
		if (udp_h->dst_port != rte_cpu_to_be_16(COORDINATOR_PORT) &&
										(pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
			return -1;
		paxos_h = (struct paxos_hdr*) ((char*)udp_h + sizeof(struct udp_hdr));

		if (rte_log_get_global_level() == RTE_LOG_DEBUG) {
			rte_hexdump(stdout, "paxos", paxos_h, sizeof(struct paxos_hdr));
			print_paxos_hdr(paxos_h);
		}
		/*convert ip address to mac address */
		dst_eth_addr.as_int = ETHER_ADDR_FOR_IPV4_MCAST(ACCEPTOR_ADDR);
		ether_addr_copy(&dst_eth_addr.as_addr, &eth_h->d_addr);
		// ether_addr_copy(&ether_acceptor_macaddr, &eth_h->d_addr);
		ip_h->dst_addr = rte_cpu_to_be_32(ACCEPTOR_ADDR);
		ip_h->src_addr = rte_cpu_to_be_32(COORDINATOR_ADDR);
		ip_h->hdr_checksum = 0;
		paxos_h->inst = rte_cpu_to_be_32(coord->cur_inst++);
		pkt->l2_len = l2_len;
		pkt->l3_len = l3_len;
		pkt->l4_len = rte_be_to_cpu_16(udp_h->dgram_len);
		pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM| PKT_TX_UDP_CKSUM;
		udp_h->dst_port = rte_cpu_to_be_16(ACCEPTOR_PORT);
		udp_h->src_port = rte_cpu_to_be_16(COORDINATOR_PORT);
		udp_h->dgram_cksum = get_psd_sum(ip_h, ETHER_TYPE_IPv4, pkt->ol_flags);
		unsigned nb_tx = rte_eth_tx_buffer(0 ,0, coord->tx_buffer, pkt);
		if(nb_tx)
			rte_atomic32_add(&tx_counter, nb_tx);
		return 0;
	}
	else
		return -1;


}
static uint16_t
add_timestamps(uint8_t port, uint16_t qidx __rte_unused,
				struct rte_mbuf **pkts, uint16_t nb_pkts,
				uint16_t max_pkts __rte_unused, void *user_param)
{
	struct coordinator* cord = (struct coordinator*) user_param;
	unsigned i;
	uint64_t now = rte_rdtsc();

	for(i = 0; i < nb_pkts; i++)
	{
		pkts[i]->udata64 = now;
		if(paxos_rx_process(port, pkts[i], cord) < 0)
			rte_pktmbuf_free(pkts[i]);
	}
	return nb_pkts;
}
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool, struct coordinator* coord)
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

	if(port >= rte_eth_dev_count())
		return -1;

	/*Configure Ethernet device*/
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if(retval < 0)
		return -1;

	/*receive rings for ethernet port*/
	for (q = 0; q < rx_rings; q++)
	{
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
					rte_eth_dev_socket_id(port), rxconf, mbuf_pool);
		if(retval < 0)
			return retval;
	}
	/*transmit rings for ethernet port*/
	for (q = 0; q < tx_rings; q++)
	{
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
					rte_eth_dev_socket_id(port), txconf);
		if(retval < 0)
			return retval;
	}

	retval = rte_eth_dev_start(port);
	if(retval < 0)
		return retval;

	rte_eth_promiscuous_enable(port);

	rte_eth_add_rx_callback(port, 0, add_timestamps, coord);
	rte_eth_add_tx_callback(port, 0, calc_latency, NULL);
	return 0;
}
static void
lcore_main(uint8_t port, struct coordinator* coord)
{
	struct rte_mbuf *pkts_burst[BURST_SIZE];
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	unsigned nb_rx;

	const uint64_t drain_tsc = (rte_get_tsc_hz() + NS_PER_S - 1 ) / NS_PER_S * BURST_TX_DRAIN_NS;

	prev_tsc = 0;

	/*Run until the application is quit or killed*/
	while(!force_quit)
	{
		cur_tsc = rte_rdtsc();
		/*TX burst queue drain*/
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc))
		{
			/*Send any packets queued up for transmission on a port and HW queue*/
			unsigned nb_tx = rte_eth_tx_buffer_flush(port, 0, coord->tx_buffer);
			if(nb_tx)
				rte_atomic32_add(&tx_counter, nb_tx);
		}
		prev_tsc = cur_tsc;
		/*Retrieve a burst of input packets from a receive queue of an Ethernet device. */
		nb_rx = rte_eth_rx_burst(port, 0, pkts_burst, BURST_SIZE);
		if (unlikely(nb_rx == 0))
			continue;
		rte_atomic32_add(&rx_counter, nb_rx);
	}
}
int main (int argc, char *argv[])
{
	uint8_t portid = 0;
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	force_quit = false;

	struct coordinator coord = {.cur_inst = 1};

	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initilization\n");

	rte_timer_init(&timer);
	uint64_t hz = rte_get_timer_hz();

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
				NUM_MBUFS, MBUF_CACHE_SIZE, 0,
				RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if(mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");

	coord.tx_buffer = rte_zmalloc_socket("tx_buffer",
					RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE), 0,
					rte_eth_dev_socket_id(portid));
	if (coord.tx_buffer == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n", (unsigned) portid);

	rte_eth_tx_buffer_init(coord.tx_buffer, BURST_SIZE);

	/*display stats every peridod seconds*/
	int lcore_id = rte_get_next_lcore(rte_lcore_id(),0,1);
	rte_timer_reset(&timer, hz, PERIODICAL, lcore_id, report_stat, NULL);
	rte_eal_remote_launch(check_timer_expiration, NULL, lcore_id);

	rte_timer_subsystem_init();

	if(port_init(portid, mbuf_pool, &coord) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n", portid);

	lcore_main(portid, &coord);

	return 0;

}