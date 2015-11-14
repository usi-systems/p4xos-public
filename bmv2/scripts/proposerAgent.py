#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor, defer
from twisted.web.server import Site, NOT_DONE_YET
import argparse
import ConfigParser
import logging
import sys
import os
import struct
THIS_DIR=os.path.dirname(os.path.realpath(__file__))
sys.path.append(THIS_DIR)

from paxoscore.proposer import Proposer
from httpServer import WebServer, MainPage

logging.basicConfig(level=logging.DEBUG,format='%(message)s')

VALUE_SIZE = 64

class Pserver(DatagramProtocol):

    def __init__(self, config):
        self.dst = (config.get('learner', 'addr'), \
                    config.getint('learner', 'port'))
        self.defers = {}

    def startProtocol(self):
        """
        Called after protocol has started listening.
        """
        pass

    def send(self, req_id, msg):
        self.transport.write(msg, self.dst)
        self.defers[req_id] = defer.Deferred()
        return self.defers[req_id]

    def addDeliverHandler(self, deliver):
        self.deliver = deliver

    def datagramReceived(self, datagram, address):
        try:
            fmt = '>' + 'B {0}s'.format(VALUE_SIZE)
            packer = struct.Struct(fmt)
            packed_size = struct.calcsize(fmt)
            unpacked_data = packer.unpack(datagram[:packed_size])
            req_id, result =  unpacked_data
            self.defers[req_id].callback(result)
            pass
        except defer.AlreadyCalledError as ex:
            #logging.error("already call")
            pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Paxos Proposer.')
    parser.add_argument('--cfg', required=True)
    args = parser.parse_args()
    config = ConfigParser.ConfigParser()
    config.read(args.cfg)
    conn = Pserver(config)
    proposer = Proposer(conn, 0)
    conn.addDeliverHandler(proposer.deliver)
    reactor.listenUDP(config.getint('client', 'port'), conn)

    root = MainPage()
    server = WebServer(proposer)
    root.putChild('get', server)
    root.putChild('put', server)
    factory = Site(root)
    reactor.listenTCP(8080, factory)

    reactor.run()
