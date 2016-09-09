#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <rte_ether.h>
#include "rte_paxos.h"

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER8
#define FATAL_ERROR(fmt, args...)       rte_exit(EXIT_FAILURE, fmt "\n", ##args)
#define PRINT_INFO(fmt, args...)        RTE_LOG(INFO, APP, fmt "\n", ##args)
#define PRINT_DEBUG(fmt, args...)       RTE_LOG(DEBUG, APP, fmt "\n", ##args)


union tunnel_offload_info {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l4_len:8; /**< L4 Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size */
		uint64_t outer_l2_len:7; /**< outer L2 Header Length */
		uint64_t outer_l3_len:16; /**< outer L3 Header Length */
	};
} __rte_cache_aligned;

void signal_handler(int signum);
void parse_ethernet(struct ether_hdr *eth_hdr, union tunnel_offload_info *info,
		uint8_t *l4_proto);
#endif
