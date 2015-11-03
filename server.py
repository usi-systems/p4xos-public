#!/usr/bin/env python
"""Paxos ProposerServer"""
__author__ = "Tu Dang"

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
import ConfigParser
import argparse
import struct
import binascii

class LearnerServer(DatagramProtocol):
    """docstring for LearnerServer"""
    def __init__(self, config, nid):
        self.config = config
        self.majority = self.config.getint('common', 'majority')

    def startProtocol(self):
        """
        Called after protocol has started listening.
        """
        self.transport.joinGroup(self.config.get('learner', 'addr'))

    def parseDatagram(self, datagram):
        fmt = '>' + 'b h b b 3s'
        packer = struct.Struct(fmt)
        packed_size = struct.calcsize(fmt)
        unpacked_data = packer.unpack(datagram[:packed_size])
        remaining_payload = datagram[packed_size:]
        typ, inst, rnd, vrnd, value = unpacked_data
        return(unpacked_data, remaining_payload)
        
    def datagramReceived(self, datagram, address):
        unpacked_data, remaining_payload  = self.parseDatagram(datagram)
        typ, inst, rnd, vrnd, value = unpacked_data
        print 'Message[%s:%d] - | %d, %d, %d, %d, %s | %s' % (address[0], address[1], 
                typ, inst, rnd, vrnd, value, remaining_payload)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--nid', type=int, required=True, help="Learner's ID")
    args = parser.parse_args()

    config = ConfigParser.ConfigParser()
    config.read('paxos.cfg')
    reactor.listenMulticast(config.getint('learner', 'port'), LearnerServer(config, args.nid),
        listenMultiple=True)
    reactor.run()
