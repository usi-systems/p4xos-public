#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <strings.h>
#include <inttypes.h>
#include <time.h>

#include "proposer.h"
#include "message.h"
#include "stat.h"
#include "netpaxos_utils.h"

#define LEARNER_PORT 34952
#define BUFSIZE 1470


void perf_cb(evutil_socket_t fd, short what, void *arg)
{
    Stat *stat = (Stat *) arg;
    if (stat->avg_lat > 0) {
        // printf("%" PRId64 "\n", stat->avg_lat);
        printf("message/second: %d, average latency: %.2f\n",
                stat->mps, ((double) stat->avg_lat) / stat->mps);
        stat->mps = 0;
        stat->avg_lat = 0;
    }
}

void recv_cb(evutil_socket_t fd, short what, void *arg)
{
    Stat *stat = (Stat *) arg;
    Message msg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0)
      perror("ERROR in recvfrom");
    Message m = decode_message(msg);
    if (stat->verbose) {
        char buf[BUFSIZE];
        message_to_string(m, buf);
        printf("%s" , buf);
    }
    stat->mps++;
    struct timeval end, result;
    gettimeofday(&end, NULL);
    if (timeval_subtract(&result, &end, &m.ts) < 0) {
        printf("Latency is negative");
    }
    int64_t latency = (int64_t) (result.tv_sec*1000000 + result.tv_usec);
    stat->avg_lat += latency;
}

void send_cb(evutil_socket_t fd, short what, void *arg)
{
    struct sockaddr_in *serveraddr = (struct sockaddr_in *) arg;
    socklen_t serverlen = sizeof(*serveraddr);
    Message msg;
    msg.mstype = 3;
    msg.inst = 10;
    msg.rnd = 31;
    msg.vrnd = 00;
    msg.acpid = 1;

    gettimeofday(&msg.ts, NULL);
    bzero(msg.value, sizeof(msg.value));
    sprintf(msg.value, "%s", "abcdefgh");

    // char buf[BUFSIZE];
    // message_to_string(msg, buf);
    // printf("%s" , buf);

    size_t msglen = sizeof(msg);
    int n = sendto(fd, &msg, msglen, 0, (struct sockaddr*) serveraddr, serverlen);
    if (n < 0)
        perror("ERROR in sendto");

    // printf("Send %d bytes\n", n);
}

int start_proposer(char* hostname, int duration, int verbose) {
    Stat stat = {verbose, 0, 0};
    struct hostent *server;
    int serverlen;
    struct sockaddr_in serveraddr;

    struct event_base *base = event_base_new();
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        return -1;
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(LEARNER_PORT);

    struct event *ev_recv, *ev_send, *ev_perf;
    struct timeval timeout = {0, duration};
    struct timeval perf_tm = {1, 0};
    ev_recv = event_new(base, fd, EV_READ|EV_PERSIST, recv_cb, &stat);
    ev_send = event_new(base, fd, EV_TIMEOUT|EV_PERSIST, send_cb, &serveraddr);
    ev_perf = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, perf_cb, &stat);
    event_add(ev_recv, NULL);
    event_add(ev_send, &timeout);
    event_add(ev_perf, &perf_tm);
    event_base_dispatch(base);
    return 0;
}