#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
import argparse
import ConfigParser
import sys
import os
THIS_DIR=os.path.dirname(os.path.realpath(__file__))
sys.path.append(THIS_DIR)

from paxoscore.proposer import Proposer

class Pserver(DatagramProtocol):

    def __init__(self, config):
        self.dst = (config.get('learner', 'addr'), \
                    config.getint('learner', 'port'))

    def startProtocol(self):
        """
        Called after protocol has started listening.
        """
        pass

    def send(self, msg):
        self.transport.write(msg, self.dst)

    def addDeliverHandler(self, deliver):
        self.deliver = deliver

    def datagramReceived(self, datagram, address):
        self.deliver(datagram)


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
    reactor.callLater(1,proposer.submitRandomValue)
    reactor.run()
