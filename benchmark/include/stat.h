#ifndef _STAT_H_
#define _STAT_H_
#include <stdint.h>

typedef struct Stat {
    int verbose;
    int mps;
    int64_t avg_lat;
} Stat;

#endif