#!/usr/bin/env python

from scapy.all import *
import sys

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  IntField("msgtype", 3),
                    IntField("inst", 1),
                    ShortField("rnd", 1),
                    ShortField("vrnd", 0),
                    IntField("acpt", 0),
                    IntField("val", 4),
                    IntField("content", 4567) ]

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

def paxos_packet(fname):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=34951, dport=0x8888)
    p0 = eth / ip / udp / Paxos(msgtype = 0)
    p0.show()
    p2a = eth / ip / udp / Paxos()
    p2a.show()
    p1a = eth / ip / udp / Paxos(msgtype=1, rnd = 2, val= 0)
    p1a.show()
    pkts = [ p0, p2a, p1a]

    wrpcap("%s" % fname, pkts)


if __name__=='__main__':
    paxos_packet(sys.argv[1])
