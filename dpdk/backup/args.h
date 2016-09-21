#ifndef _ARGS_H_
#define _ARGS_H_

#include <rte_log.h>



struct {
    int nb_acceptors;
    int nb_learners;
    int learner_id;
    int level_db;
} learner_config;

void print_usage(const char *prgname);
void parse_args(int argc, char **argv);

#endif