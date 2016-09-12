#ifndef _ARGS_H_
#define _ARGS_H_

#include <rte_log.h>

struct {
    int acceptor_id;
} acceptor_config;

void print_usage(const char *prgname);
void parse_args(int argc, char **argv);

#endif