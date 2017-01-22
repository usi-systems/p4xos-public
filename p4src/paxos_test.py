#!/usr/bin/env python

from scapy.all import *
import sys
import argparse


class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[
        XShortField("msgtype", 0x3),
        XIntField("inst", 0x0),
        XShortField("rnd", 0x0),
        XShortField("vrnd", 0x0),
        XShortField("acpt", 0x0),
        XIntField("valuelen", 32),
        XBitField("value", 0x11223344, 256)
    ]

bind_layers(UDP, Paxos, dport=0x8888)


def paxos_packet(inst, rnd, vrnd, acpt, typ, val):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="192.168.4.95", dst="224.3.29.73")
    udp = UDP(sport=34951, dport=0x8888)
    pkt = eth / ip / udp / Paxos(inst=inst, rnd=rnd, vrnd=vrnd, acpt=acpt, msgtype=typ, value=val)
    return pkt


def handle(x):
    x.show()


def server(args):
    sniff(iface = args.interface, filter= 'udp', prn = lambda x: handle(x))

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument('interface', help='Network interface')
    parser.add_argument('-i', '--inst', help='Paxos instance', type=int, default=0)
    parser.add_argument('-t', '--type', help='Paxos type', type=int, default=3)
    parser.add_argument('-r', '--rnd', help='Paxos round', type=int, default=0)
    parser.add_argument('-d', '--vrnd', help='Paxos value round', type=int, default=0)
    parser.add_argument('-v', '--value', help='Paxos value', default=0x11223344)
    parser.add_argument('-a', '--acpt', help='Paxos acceptor id', type=int, default=0)
    parser.add_argument('-c', '--count', help='send multiple packets', type=int, default=1)
    parser.add_argument('-o', '--output', help='output pcap file', type=str)
    parser.add_argument('-s', '--server', help='run server mode', action='store_true', default=False)
    args = parser.parse_args()

    if args.server:
        server(args)
    else:
        pkt = paxos_packet(args.inst, args.rnd, args.vrnd, args.acpt, args.type, args.value)
        pkt.show()
        sendp(pkt, iface=args.interface, count=args.count)
        # wrpcap("%s" % args.output, pkt)
