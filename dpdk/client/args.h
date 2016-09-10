#ifndef _ARGS_H_
#define _ARGS_H_

#include <rte_log.h>


enum PAXOS_TEST { PROPOSER, COORDINATOR, ACCEPTOR, LEARNER };

struct {
    enum PAXOS_TEST test;
    int period;
} client_config;

void print_usage(const char *prgname);
void parse_args(int argc, char **argv);

#endif