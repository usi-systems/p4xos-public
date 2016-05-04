#!/usr/bin/env python

from scapy.all import *
import sys
import argparse
import datetime


class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  XIntField("inst", 0x1),
                    XShortField("rnd", 0x1),
                    XShortField("vrnd", 0x0),
                    XShortField("acpt", 0x0),
                    XShortField("msgtype", 0x3) ]

def handle(x):
    x.show()
    pax = Paxos(x['Raw'].load)

    pax.show()

def server(args):
    sniff(iface = args.interface, filter= 'dst %s and udp' % args.address, prn = lambda x: handle(x))

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument("interface", help="bind to specified interface")
    parser.add_argument("--address", default="224.3.29.73", help="destination IP address")
    args = parser.parse_args()

    server(args)
