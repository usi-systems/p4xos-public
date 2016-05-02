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

def server(itf):
    sniff(iface = itf, prn = lambda x: handle(x))

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument("interface", help="bind to specified interface")
    args = parser.parse_args()

    server(args.interface)
