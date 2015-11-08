#!/usr/bin/python
from scapy.all import *

import struct
import sys

paxos_type = { 1: "prepare", 2: "promise", 3: "accept", 4: "accepted" }

def handle_pkt(pkt):
    try:
        if pkt['IP'].proto != 0x11:
            return
        datagram = pkt['Raw'].load
        fmt = '>' + 'b h b b 3s'
        packer = struct.Struct(fmt)
        packed_size = struct.calcsize(fmt)
        unpacked_data = packer.unpack(datagram[:packed_size])
        remaining_payload = datagram[packed_size:]
        typ, inst, rnd, vrnd, value = unpacked_data
        print "| %10s | %4d |  %02x | %02x | %04s | %s |" % \
                (paxos_type[typ], inst, rnd, vrnd, value, remaining_payload)
    except IndexError as ex:
        print ex


def main():
    print "| %10s | %4s |  %2s | %2s | %4s | %s |" % \
         ("type", "inst", "pr", "ar", "val", "payload")
    sniff(filter="udp && dst port 34952",
          prn = lambda x: handle_pkt(x))

if __name__ == '__main__':
    main()
