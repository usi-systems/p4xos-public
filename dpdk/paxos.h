#ifndef _PAXOS_H_
#define _PAXOS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct paxos_hdr {
	uint16_t msgtype;
	uint32_t inst;
	uint16_t rnd;
	uint16_t vrnd;
	uint16_t acptid;
	uint8_t paxosval[32];
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* PAXOS_H_ */
