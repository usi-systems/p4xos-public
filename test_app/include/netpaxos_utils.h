#ifndef _NETPAXOS_UTILS_H_
#define _NETPAXOS_UTILS_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

void gettime(struct timespec * ts);
int64_t timediff(struct timespec *start, struct timespec *end);
int compare_ts(struct timespec *time1, struct timespec *time2);

#endif