#!/usr/bin/env python

from scapy.all import *
import sys
import argparse

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  XIntField("inst", 0x1),
                    XShortField("rnd", 0x1),
                    XShortField("vrnd", 0x0),
                    XShortField("acpt", 0x0),
                    XShortField("msgtype", 0x3),
                    XIntField("val", 0x11223344) ]


def paxos_packet(typ, inst, rnd, vrnd, value):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=34951, dport=0x8888)
    pkt = eth / ip / udp / Paxos(msgtype=typ, inst=inst, rnd=rnd, vrnd=vrnd, val=value)
    return pkt

def udp_packet():
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=12642, dport=12334)
    pkt = eth / ip / udp / "You're fool"
    return pkt

def client(args):
    pkt  = paxos_packet(args.pxtype, args.inst, args.rnd, args.vrnd, args.value)
    if args.interface:
        sendp(pkt, iface= args.interface, inter=args.inter, count=args.count)
    return pkt

def handle(x):
    pax = Paxos(x['Raw'].load)
    pax.show()
    print pax.inst

def server(itf):
    sniff(iface = itf, prn = lambda x: handle(x))

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument('--inst', help='Paxos instance', type=int, default=0)
    parser.add_argument('-r', '--rnd', help='Paxos round', type=int, default=1)
    parser.add_argument('-a', '--vrnd', help='Paxos value round', type=int, default=0)
    parser.add_argument('-t', '--pxtype', help='Paxos msg type', type=int, default=0)
    parser.add_argument('--count', help='Number of packets to send', type=int, default=1)
    parser.add_argument('--inter', help='Interval between sending', type=float, default=1)
    parser.add_argument('-v', '--value', help='Paxos value', type=int, default=0x11223344)
    parser.add_argument("-s", "--server", help="run as server", action="store_true")
    parser.add_argument("-i", "--interface", help="bind to specified interface")
    parser.add_argument("-o", "--output", required=True, help="output filename")
    parser.add_argument("-u", "--udp", help="generate an udp packet", action="store_true")
    args = parser.parse_args()

    if args.server:
        server(args.interface)
    else:
        if args.udp:
            pkt = udp_packet()
        else:
            pkt = client(args)
        
        if args.output:
            wrpcap(args.output, pkt)
