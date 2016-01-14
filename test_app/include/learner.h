#ifndef _LEARNER_H
#define _LEARNER_H

typedef struct Stat {
    int mps;
    float avg_lat;
} Stat;

int start_learner();
#endif