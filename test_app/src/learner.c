#include "learner.h"
#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <strings.h>
#include "message.h"

/* Here's a callback function that calls loop break */
#define LEARNER_PORT 34952
#define BUFSIZE 1470

int64_t timediff(struct timespec *start, struct timespec *end)
{
    int64_t result = (1e9 * (end->tv_sec - start->tv_sec) +
                    end->tv_nsec - start->tv_nsec);
    if (result > 0)
        return result;
    else
        return 0;
}


void monitor(evutil_socket_t fd, short what, void *arg) {
    Stat *stat = (Stat *) arg;
    if (stat->avg_lat > 0) {
        printf("message/second: %d, average latency: %.2f\n", stat->mps, ((double) stat->avg_lat) / stat->mps);
        stat->mps = 0;
        stat->avg_lat = 0;
    }

}


void cb_func(evutil_socket_t fd, short what, void *arg)
{
    Stat *stat = (Stat *) arg;
    Message msg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0)
      perror("ERROR in recvfrom");
    // printf("Received %d bytes from %s:%d\n", n, inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
    char buf[BUFSIZE];
    Message m = decode_message(msg);
    message_to_string(m, buf);
    // printf("%s" , buf);
    stat->mps++;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    int64_t latency = timediff(&m.ts, &end);
    stat->avg_lat += latency;
}

int start_learner() {
    struct event_base *base = event_base_new();
    Stat stat = {0, 0.0};

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(LEARNER_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    // bindSocketToDevice(fd, "eth2");
    if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        perror("ERROR on binding");
    struct event *recv_ev, *monitor_ev;
    recv_ev = event_new(base, fd, EV_READ|EV_PERSIST, cb_func, &stat);
    struct timeval timeout = {1, 0};
    monitor_ev = event_new(base, 0, EV_TIMEOUT|EV_PERSIST, monitor, &stat);
    event_add(recv_ev, NULL);
    event_add(monitor_ev, &timeout);
    event_base_dispatch(base);
    return 0;
}