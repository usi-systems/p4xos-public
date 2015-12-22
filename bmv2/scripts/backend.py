#!/usr/bin/python

import os
import sys
import logging
import argparse
import ConfigParser
from scapy.all import *
from paxoscore.learner import Learner
from twisted.internet import defer


THIS_DIR=os.path.dirname(os.path.realpath(__file__))
sys.path.append(THIS_DIR)

class SimpleDatabase(object):
    """
    Simple database backend
    """
    def __init__(self):
        self.db = {}

    def execute(self, cmd, d):
        if cmd['action'] == 'put':
            k = cmd['key'][0]
            v = cmd['value'][0]
            self.db[k] = v
            d.callback("Success\n")

        elif cmd['action'] == 'get':
            k = cmd['key'][0]
            try:
                v = self.db.get(k)
                d.callback('%s\n' % v)
            except KeyError as ex:
                d.callback("None\n")


def main():
    parser = argparse.ArgumentParser(description='Paxos Proposer.')
    parser.add_argument('--cfg', required=True)
    args = parser.parse_args()
    config = ConfigParser.ConfigParser()
    config.read(args.cfg)

    num_acceptors = config.getint('common', 'num_acceptors')
    learner_addr = config.get('learner', 'addr')
    learner_port = config.getint('learner', 'port')
    count = config.getint('instance', 'count')
    timeout = config.getint('timeout', 'second')

    learner = Learner(num_acceptors, learner_addr, learner_port)
    dbserver = SimpleDatabase()
    learner.addDeliver(dbserver.execute)
    try:
        learner.start(count, timeout)
    except (KeyboardInterrupt, SystemExit):
        learner.stop()
        sys.exit()

if __name__ == '__main__':
    main()
