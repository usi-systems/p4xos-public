#ifndef _DPDK_PROPOSER_H_
#define _DPDK_PROPOSER_H_

#include <rte_mempool.h>
#include <rte_atomic.h>
#include "paxos.h"
#include "proposer.h"

static const struct ether_addr ether_multicast = {
    .addr_bytes= { 0x01, 0x1b, 0x19, 0x0, 0x0, 0x0 }
};


static rte_atomic32_t stat = RTE_ATOMIC32_INIT(0);

/* test proposer propose */
static char command[11] = "Hello DPDK";

struct rte_mempool *mbuf_pool;

void proposer_preexecute(struct proposer *p);
void try_accept(struct proposer *p);
void proposer_handle_promise(struct proposer *p, struct paxos_promise *promise);
void proposer_handle_accepted(struct proposer *p, struct paxos_accepted *ack);
void send_paxos_message(paxos_message *pm);
#endif