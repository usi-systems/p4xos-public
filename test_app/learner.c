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
#define LEARNER_PORT 35492
#define BUFSIZE 1470

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    Message msg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0) 
      perror("ERROR in recvfrom");
    printf("Received packet from %s:%d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
    
    char buf[BUFSIZE];
    message_to_string(msg, buf);
    printf("%s" , buf);
}

int start_learner() {
    struct event_base *base = event_base_new();
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(LEARNER_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        perror("ERROR on binding");
    struct event *ev1;
    ev1 = event_new(base, fd, EV_READ|EV_PERSIST, cb_func, NULL);
    event_add(ev1, NULL);
    event_base_dispatch(base);
    return 0;
}