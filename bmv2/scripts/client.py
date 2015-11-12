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
import os

VALUE_SIZE = 64 

THIS_DIR=os.path.dirname(os.path.realpath(__file__))

class Client(DatagramProtocol):

    def __init__(self, config, args):
        self.config = config
        self.args = args
        self.dst = (config.get('learner', 'addr'), config.getint('learner', 'port'))


    def startProtocol(self):
        """
        Called after protocol has started listening.
        """
        num_instances = self.config.getint('instance', 'count')
        for i in range(num_instances):
            self.sendPacket()

    def sendPacket(self):
        v = ''.join(random.SystemRandom().choice(string.ascii_uppercase + \
                    string.digits) for _ in range(VALUE_SIZE))
        values = (args.typ, args.inst, args.rnd, args.vrnd, v)
        packer = struct.Struct('>' + 'B H B B {0}s'.format(VALUE_SIZE))
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
    config.read('%s/paxos.cfg' % THIS_DIR)
    reactor.listenUDP(config.getint('client', 'port'), Client(config, args))
    timeout = config.getint('timeout', 'second')
    reactor.callLater(timeout, reactor.stop)
    reactor.run()
