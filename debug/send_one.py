#!/usr/bin/env python

from scapy.all import *
import sys


eth = Ether(dst="08:00:27:10:a8:80")
ip = IP(src="10.0.0.1", dst="10.0.0.2")
udp = UDP(sport=34951, dport=4830)
tcp = TCP(sport=34951, dport=4830)

data = eth / ip / TCP() / "Hello"
if sys.argv[2] == 'arp':
    data = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(pdst="192.168.1.2")
elif sys.argv[2] == 'tcp':
    data = eth / ip / tcp
elif sys.argv[2] == 'udp':
    data = eth / ip / udp
else:
    data = Ether() / IP ()
# p.show()
data.show()
sendp(data, iface = sys.argv[1])