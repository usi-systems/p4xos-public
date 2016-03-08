#!/usr/bin/env python

from scapy.all import *
import sys
import argparse

class PaxosValue(Packet):
    name ="PaxosValue "
    fields_desc =[
                    XBitField("value", 0x11223344, 256)
                ]

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  
                    XIntField("instance", 0x1),
                    XShortField("round", 0x1),
                    XShortField("vround", 0x0),
                    XIntField("acceptor", 0x0),
                    XShortField("msgtype", 0x3),
                ]


def paxos_packet(typ, inst, rnd, vrnd, value):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=34951, dport=0x8888)
    pkt = eth / ip / udp / Paxos(msgtype=typ, instance=inst, round=rnd, vround=vrnd) / fuzz(PaxosValue())
    return pkt



if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument('-i', '--inst', help='Paxos instance', type=int, default=0)
    parser.add_argument('-r', '--rnd', help='Paxos round', type=int, default=1)
    parser.add_argument('-a', '--vrnd', help='Paxos value round', type=int, default=0)
    parser.add_argument('-v', '--value', help='Paxos value', type=int, default=0x11223344)
    parser.add_argument('-o', '--output', help='output pcap file', type=str, required=True)
    args = parser.parse_args()

    p0  = paxos_packet(0, args.inst, args.rnd, args.vrnd, args.value)
    p2a = paxos_packet(3, args.inst, args.rnd, args.vrnd, args.value)
    p1a = paxos_packet(1, args.inst, args.rnd, args.vrnd, 0)
    p1b = paxos_packet(2, args.inst, args.rnd, args.vrnd, 0xAABBCCDD)

    pkts = [p0, p2a, p1a, p1b]
    wrpcap("%s" % args.output, pkts)
