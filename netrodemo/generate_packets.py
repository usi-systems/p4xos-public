#!/usr/bin/env python

from scapy.all import *
import sys
import argparse

class Value(Packet):
    name = "PaxosValue"
    fields_desc = [ IntField("content", 4567) ]

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc = [  IntField("msgtype", 3),
                    IntField("inst", 1),
                    ShortField("rnd", 1),
                    ShortField("vrnd", 0),
                    IntField("acpt", 0),
                    IntField("valsize", 4) ]

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

def paxos_packet(args):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=34951, dport=0x8888)
    pkt = eth / ip / udp / Paxos(msgtype = args.typ, inst=args.inst, rnd=args.rnd, vrnd=args.vrnd,
                                valsize=args.valsize)
    if (args.valsize > 0):
        pkt = pkt / Value(content = args.value)

    pkt.show()
    wrpcap("%s" % args.output, pkt)


if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument('-t', '--typ', help='Paxos message Type', type=int, default=0)
    parser.add_argument('-i', '--inst', help='Paxos instance', type=int, default=0)
    parser.add_argument('-r', '--rnd', help='Paxos round', type=int, default=1)
    parser.add_argument('-a', '--vrnd', help='Paxos value round', type=int, default=0)
    parser.add_argument('-s', '--valsize', help='Paxos value round', type=int, default=4)
    parser.add_argument('-v', '--value', help='Paxos value', type=int, default=0x1234)
    parser.add_argument('-o', '--output', help='output pcap file', type=str, required=True)
    args = parser.parse_args()

    paxos_packet(args)
