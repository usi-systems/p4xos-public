#!/usr/bin/python
from scapy.all import *

class Paxos(Packet):
    name = "Paxos"
    fields_desc = [
        XByteField('pxtype', 1),
        ShortField('instance', 1),
        XByteField('round', 1),
        XByteField('vround', 1),
        StrFixedLenField('value', 'CIAO', 4)
    ]
