#ifndef _RTE_PAXOS_H_
#define _RTE_PAXOS_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <rte_mbuf.h>
#include "paxos.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t TIMER_RESOLUTION_CYCLES;
struct paxos_hdr {
	uint16_t msgtype;
	uint32_t inst;
	uint16_t rnd;
	uint16_t vrnd;
	uint16_t acptid;
	uint32_t value_len;
	uint8_t paxosval[32];
} __attribute__((__packed__));

void print_paxos_hdr(struct paxos_hdr *p);
uint16_t get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags);
void craft_new_packet(struct rte_mbuf **created_pkt, uint32_t srcIP,
        uint32_t dstIP, uint16_t sport, uint16_t dport, size_t data_size,
        uint8_t output_port);
void add_paxos_message(struct paxos_message *pm, struct rte_mbuf *created_pkt,
                        uint16_t sport, uint16_t dport, uint32_t dstIP);
void send_batch(struct rte_mbuf **mbufs, int count, int port_id);
uint16_t calc_latency(uint8_t port __rte_unused, uint16_t qidx __rte_unused,
        struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused);
int check_timer_expiration(__attribute__((unused)) void *arg);
void print_addr(struct sockaddr_in* s);


#ifdef __cplusplus
}
#endif

#endif /* PAXOS_H_ */
