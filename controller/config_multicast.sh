#!/bin/sh

# IP address: 1st argument
ip addr add $1/24 dev enp2s0
ip link set enp1s0 multicast off
ip link set enp2s0 multicast on
ip route add 224.0.0.0/4 dev enp2s0
echo 1 >  /proc/sys/net/ipv4/ip_forward