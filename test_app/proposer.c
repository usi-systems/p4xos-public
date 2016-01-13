#include "proposer.h"
#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <strings.h>
#include "message.h"

#define LEARNER_PORT 35492
#define BUFSIZE 1470
/* Here's a callback function that calls loop break */
void recv_cb(evutil_socket_t fd, short what, void *arg)
{
    printf("Callback called\n");
}

void send_cb(evutil_socket_t fd, short what, void *arg)
{
    struct sockaddr_in *serveraddr = (struct sockaddr_in *) arg;
    socklen_t serverlen = sizeof(*serveraddr);
    Message msg;
    msg.mstype = '1';
    msg.inst = 10;
    msg.rnd = '1';
    msg.vrnd = '0';
    sprintf(msg.acpid, "%8s", "69701832");
    sprintf(msg.value, "%s", "8364XVJF");

    size_t msglen = sizeof(msg);
    int n = sendto(fd, &msg, msglen, 0, (struct sockaddr*) serveraddr, serverlen);
    if (n < 0)
        perror("ERROR in sendto");

    printf("Send %d bytes\n", n);
}

int start_proposer(char* hostname) {
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

    struct event *ev_recv, *ev_send;
    struct timeval timeout = {0,1000000};
    ev_recv = event_new(base, fd, EV_READ|EV_PERSIST, recv_cb, NULL);
    ev_send = event_new(base, fd, EV_TIMEOUT|EV_PERSIST, send_cb, &serveraddr);
    event_add(ev_recv, NULL);
    event_add(ev_send, &timeout);
    event_base_dispatch(base);
    return 0;
}