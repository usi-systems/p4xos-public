CC=gcc
CFLAGS=-Wall -pthread -std=c99
LIB=

all: netpaxos_controller dpdk_controller dpdk_l3_controller dpdk_l3-full_controller dpdk_l2_l3_controller

netpaxos_controller:
	$(CC) $(CFLAGS) $(LIB) handlers.c controller.c messages.c sock_helpers.c threadpool.c fifo.c netpaxos_controller.c -o netpaxos_controller

dpdk_controller:
	$(CC) $(CFLAGS) $(LIB) handlers.c controller.c messages.c sock_helpers.c threadpool.c fifo.c dpdk_controller.c -o dpdk_controller

dpdk_l3_controller:
	$(CC) $(CFLAGS) $(LIB) handlers.c controller.c messages.c sock_helpers.c threadpool.c fifo.c dpdk_l3_controller.c -o dpdk_l3_controller

dpdk_l3-full_controller:
	$(CC) $(CFLAGS) $(LIB) handlers.c controller.c messages.c sock_helpers.c threadpool.c fifo.c dpdk_l3-full_controller.c -o dpdk_l3-full_controller

dpdk_l2_l3_controller:
	$(CC) $(CFLAGS) $(LIB) handlers.c controller.c messages.c sock_helpers.c threadpool.c fifo.c dpdk_l2_l3_controller.c -o dpdk_l2_l3_controller

clean:
	rm -f netpaxos_controller dpdk_controller dpdk_l3_controller dpdk_l3-full_controller dpdk_l2_l3_controller
