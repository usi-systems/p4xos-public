#!/usr/bin/env python

from scapy.all import *
import sys
import argparse
import datetime

class Tutime(Packet):
    name ="PaxosPacket "
    fields_desc =[IntField("hour", 0x0),
                    IntField("minute", 0x0),
                    IntField("second", 0x0),
                    IntField("microsecond", 0x0) ]

def newTime(ts):
    ts = Tutime(hour=ts.hour, minute=ts.minute, second=ts.second, microsecond=ts.microsecond)
    return ts

class PaxosValue(Packet):
    name ="PaxosValue "
    fields_desc =[
                    XBitField("value", 0x11223344,256)
                ]

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  XIntField("inst", 0x1),
                    XShortField("rnd", 0x1),
                    XShortField("vrnd", 0x0),
                    XShortField("acpt", 0x0),
                    XShortField("msgtype", 0x3) ]


PAXOS_SIZE = len(Paxos())
VALSIZE = len(PaxosValue())
TIMESIZE = len(Tutime())

def handle(x, itf):
    if Raw not in x or len(x.load) != PAXOS_SIZE + VALSIZE + TIMESIZE:
        return
    pax = Paxos(x['Raw'].load)
    if Raw not in pax or len(pax.load) != VALSIZE + TIMESIZE:
        return
    paxval = PaxosValue(pax['Raw'].load)
    if Raw not in paxval or len(paxval.load) != TIMESIZE:
        return
    start = Tutime(paxval['Raw'].load)
    if start.hour > 24:
        return
    end_time = datetime.datetime.now()
    start_time = datetime.datetime(end_time.year, end_time.month, end_time.day,
        start.hour, start.minute, start.second, start.microsecond)
    dur =  end_time - start_time
    print dur.total_seconds()
    sendp(x, iface=itf)

def server(itf):
    sniff(iface = itf, prn = lambda x: handle(x, itf))

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='P4Paxos demo')
    parser.add_argument("-i", "--interface", required=True, help="bind to specified interface")
    parser.add_argument("-I", "--infile", help="input file")
    args = parser.parse_args()

    if args.infile:
        pkts = rdpcap(args.infile)
        for p in pkts:
            handle(p)
    else:
        server(args.interface)
