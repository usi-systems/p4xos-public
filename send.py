#!/usr/bin/python

from scapy.all import *
from paxos_header import *
import argparse

import sys

def make_paxos(typ, i, ipv4_dst, rnd, vrnd, val):
    eth = Ether(src='00:04:00:00:00:01', dst='00:04:00:00:00:00')
    ip = IP(dst=ipv4_dst)/UDP(sport=12934, dport=34952)
    paxos = Paxos(pxtype=typ, instance=i, round=rnd, vround=vrnd, value=val)
    return eth/ip/paxos

def main():
    #sendp("I'm travelling on Ethernet", iface="h2-eth0", loop=1, inter=0.2)        
    #msg = raw_input("What do you want to send: ")
    parser = argparse.ArgumentParser(description='Generate Paxos messages.')
    parser.add_argument('--dst', default='10.0.0.1')
    parser.add_argument('--itf', default='eth0')
    parser.add_argument('--typ', type=int, default=1)
    parser.add_argument('--rnd', type=int, default=1)
    parser.add_argument('--vrnd', type=int, default=1)
    parser.add_argument('--value', default='Ciao')
    args = parser.parse_args()

    msg = "P4 says NetPaxos"
    for i in range(8, 12):
        p = make_paxos(args.typ, i, args.dst, args.rnd, args.vrnd, args.value)/msg
        sendp(p, iface = args.itf)
    print p.show()
    # print msg

if __name__ == '__main__':
    main()
