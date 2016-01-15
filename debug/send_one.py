#!/usr/bin/env python

from scapy.all import *
import sys


eth = Ether(src=" 7e:a9:71:40:ef:9c", dst="1e:1a:3c:ae:5f:95")
ip = IP(src="10.0.0.1", dst="10.0.0.2")
udp = UDP(sport=34951, dport=4830)

data = eth / ip / TCP() / "Hello"
if sys.argv[2] == 'arp':
    data = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(pdst="192.168.1.0/24")
# p.show()
data.show()
sendp(data, iface = sys.argv[1])