#!/usr/bin/env python

from scapy.all import *
import sys
import argparse

class Value(Packet):
    name = "PaxosValue"
    fields_desc = [ XIntField("content", 0x11223344) ]

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  XIntField("inst", 0x1),
                    XShortField("rnd", 0x1),
                    XShortField("vrnd", 0x0),
                    XIntField("acpt", 0x0),
                    XShortField("msgtype", 0x3),
                    XShortField("valsize", 0x4) ]

def normal_packets(fname):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    ip2 = IP(src="10.0.0.1", dst="10.0.0.100")
    udp = eth / ip / UDP(sport=34951, dport=4830)
    tcp = eth / ip / TCP(sport=34951, dport=4830)
    arp = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(pdst="192.168.1.2")

    pkts = [eth, ip, ip2, udp, tcp, arp]
    # sendp(pkts, iface = sys.argv[1])
    wrpcap("%s" % fname, pkts)

def paxos_packet(typ, inst, rnd, vrnd, valsize, value):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=34951, dport=0x8888)
    pkt = eth / ip / udp / Paxos(msgtype=typ, inst=inst, rnd=rnd, vrnd=vrnd, valsize=valsize)
    if (args.valsize > 0):
        pkt = pkt / Value(content = value)
    return pkt



if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument('-i', '--inst', help='Paxos instance', type=int, default=0)
    parser.add_argument('-r', '--rnd', help='Paxos round', type=int, default=1)
    parser.add_argument('-a', '--vrnd', help='Paxos value round', type=int, default=0)
    parser.add_argument('-s', '--valsize', help='Paxos value round', type=int, default=4)
    parser.add_argument('-v', '--value', help='Paxos value', type=int, default=0x11223344)
    parser.add_argument('-o', '--output', help='output pcap file', type=str, required=True)
    args = parser.parse_args()

    p0  = paxos_packet(0, args.inst, args.rnd, args.vrnd, args.valsize, args.value)
    p2a = paxos_packet(3, args.inst, args.rnd, args.vrnd, args.valsize, args.value)
    p1a = paxos_packet(1, args.inst, args.rnd, args.vrnd, 0, 0)

    pkts = [p0, p2a, p1a]
    wrpcap("%s" % args.output, pkts)
