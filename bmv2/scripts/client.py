#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
import argparse
import ConfigParser
import random
import string
import struct
import binascii

class Client(DatagramProtocol):

    def __init__(self, config, args):
        self.config = config
        self.args = args
        self.dst = (config.get('learner', 'addr'), config.getint('learner', 'port'))


    def startProtocol(self):
        """
        Called after protocol has started listening.
        """
        values = (args.typ, args.inst, args.rnd, args.vrnd, args.value)
        packer = struct.Struct('>' + 'b h b b 3s')
        packed_data = packer.pack(*values)
        print 'Original values:', values
        print 'Format string  :', packer.format
        print 'Uses           :', packer.size, 'bytes'
        print 'Packed Value   :', binascii.hexlify(packed_data)
        self.transport.write(packed_data, self.dst)

    def datagramReceived(self, datagram, address):
        print datagram


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate Paxos messages.')
    parser.add_argument('--typ', type=int, default=3)
    parser.add_argument('--inst', type=int, default=1)
    parser.add_argument('--rnd', type=int, default=1)
    parser.add_argument('--vrnd', type=int, default=1)
    parser.add_argument('--value', default='yea')
    args = parser.parse_args()

    config = ConfigParser.ConfigParser()
    config.read('paxos.cfg')
    reactor.listenUDP(config.getint('client', 'port'), Client(config, args))
    reactor.callLater(1, reactor.stop)
    reactor.run()
