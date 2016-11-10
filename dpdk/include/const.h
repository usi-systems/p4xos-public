#ifndef _CONST_H_
#define _CONST_H_

#include <stdbool.h>

/* number of elements in the mbuf pool */
#define NUM_MBUFS 4096
/* Size of the per-core object cache */
#define MBUF_CACHE_SIZE 250
/* Maximum number of packets in sending or receiving */
#define BURST_SIZE 32

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

/* 1 acceptor for debugging */
#define NUM_ACCEPTORS 1

#define BUFSIZE 1500

#define PROPOSER_PORT    34952
#define COORDINATOR_PORT 34951
#define ACCEPTOR_PORT    34952
#define LEARNER_PORT     34953

// #define COORDINATOR_ADDR IPv4(224,3,29,73)
#define COORDINATOR_ADDR IPv4(10,0,0,10)
#define ACCEPTOR_ADDR IPv4(224,3,29,73)
// #define ACCEPTOR_ADDR IPv4(10,0,0,12)
#define LEARNER_ADDR IPv4(224,3,29,73)
volatile bool force_quit;

#endif
