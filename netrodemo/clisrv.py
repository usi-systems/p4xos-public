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
                    XBitField("value", 0x11223344, 256)
                ]

class Paxos(Packet):
    name ="PaxosPacket "
    fields_desc =[  XIntField("inst", 0x1),
                    XShortField("rnd", 0x1),
                    XShortField("vrnd", 0x0),
                    XIntField("acpt", 0x0),
                    XShortField("msgtype", 0x3) ]


def paxos_packet(typ, inst, rnd, vrnd, value):
    eth = Ether(dst="08:00:27:10:a8:80")
    ip = IP(src="10.0.0.1", dst="10.0.0.2")
    udp = UDP(sport=34951, dport=0x8888)
    pkt = eth / ip / udp / Paxos(inst=inst, rnd=rnd, vrnd=vrnd, msgtype=typ) / fuzz(PaxosValue())
    return pkt

def client(args):
    paxos  = paxos_packet(args.pxtype, args.inst, args.rnd, args.vrnd, args.value)
    now = datetime.datetime.now()
    start = newTime(now)
    pkt = paxos / start
    if args.outfile:
        wrpcap(args.outfile, pkt)
    else:
        sendp(pkt, iface= args.interface, count=args.count, inter=args.inter)

PAXOS_SIZE = len(Paxos())
VALSIZE = len(PaxosValue())
TIMESIZE = len(Tutime())

def handle(x):
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
    parser.add_argument("-c", "--client", help="run as client", action="store_true")
    parser.add_argument("-i", "--interface", required=True, help="bind to specified interface")
    parser.add_argument("-I", "--infile", help="input file")
    parser.add_argument("-O", "--outfile", help="output file")
    args = parser.parse_args()

    if args.infile:
        pkts = rdpcap(args.infile)
        for p in pkts:
            handle(p)
    else:
        if args.client:
            client(args)
        else:
            server(args.interface)
