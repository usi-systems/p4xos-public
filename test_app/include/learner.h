#ifndef _LEARNER_H
#define _LEARNER_H
#include <sys/types.h>

typedef struct Stat {
    int verbose;
    int mps;
    int64_t avg_lat;
} Stat;

int start_learner();
#endif