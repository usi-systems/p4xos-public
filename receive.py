#!/usr/bin/python
from scapy.all import *
from paxos_header import *

import struct
import sys

def handle_pkt(pkt):
    print pkt.show2()
    type_id = ord(pkt[Raw].load[0])
    inst = "".join("{:02x}".format(ord(c)) for c in str(pkt[Raw].load[1:3]))
    inst = int(inst, 16)
    rnd = ord(pkt[Raw].load[3])
    vrnd = ord(pkt[Raw].load[4])
    value = str(pkt[Raw].load[4:9])
    payload = pkt[Raw].load[9:]
    paxos_type = { 1: "prepare", 2: "promise", 3: "accept", 4: "accepted" }
    #print "| %10s | %4d |  %02x | %02x | %04s | %s |" % (paxos_type[type_id], inst, rnd, vrnd, value, payload)

def main():
    print "| %10s | %4s |  %2s | %2s | %4s | %s |" % ("type", "inst", "pr", "ar", "val", "payload")
    sniff(filter="udp && dst port 34952", iface = sys.argv[1],
          prn = lambda x: handle_pkt(x))

if __name__ == '__main__':
    main()
