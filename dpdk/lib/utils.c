#include <signal.h>
#include <rte_ip.h>
#include <rte_log.h>
#include "utils.h"
#include "const.h"
#include "paxos.h"
/**
 * Parse an ethernet header to fill the ethertype, outer_l2_len, outer_l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header.
 */
void
parse_ethernet(struct ether_hdr *eth_hdr, union tunnel_offload_info *info,
		uint8_t *l4_proto)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	uint16_t ethertype;

	info->outer_l2_len = sizeof(struct ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == ETHER_TYPE_VLAN) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
		info->outer_l2_len  += sizeof(struct vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	switch (ethertype) {
	case ETHER_TYPE_IPv4:
		ipv4_hdr = (struct ipv4_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct ipv4_hdr);
		*l4_proto = ipv4_hdr->next_proto_id;
		break;
	case ETHER_TYPE_IPv6:
		ipv6_hdr = (struct ipv6_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct ipv6_hdr);
		*l4_proto = ipv6_hdr->proto;
		break;
	default:
		info->outer_l3_len = 0;
		*l4_proto = 0;
		break;
	}
}

void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_USER8,
				"\n\nSignal %d received, preparing to exit...\n", signum);
		force_quit = true;
	}
}

__attribute((unused))
static void
dump_paxos_message(paxos_message *m) {
	printf("{ .type = %d, .iid = %d, .ballot = %d, .value_ballot = %d, .value = { .len = %d, val = %s}}\n",
			m->type, m->u.accept.iid, m->u.accept.ballot, m->u.accept.value_ballot,
			m->u.accept.value.paxos_value_len,
			m->u.accept.value.paxos_value_val);
}

__attribute((unused))
static void
dump_accept_message(paxos_accept *m) {
	printf("dump_accept_message\n");
	printf("ACCEPT: .iid = %d, .ballot = %d, .value_ballot = %d, .value = ",
			m->iid, m->ballot, m->value_ballot);
	if ((m->value.paxos_value_val) != NULL) {
		printf(" {.len=%d, val=%s}\n",
				m->value.paxos_value_len,
				m->value.paxos_value_val);
	} else printf("NULL }\n");
}

__attribute((unused))
static void
dump_promise_message(paxos_promise *m) {
	printf("dump_promise_message\n");
	printf("PROMISE: .iid = %d, .ballot = %d, .value_ballot = %d, .value = ",
			m->iid, m->ballot, m->value_ballot);
	if ((m->value.paxos_value_val) != NULL) {
		printf(" {.len=%d, val=%s}\n",
				m->value.paxos_value_len,
				m->value.paxos_value_val);
	} else printf("NULL }\n");
}