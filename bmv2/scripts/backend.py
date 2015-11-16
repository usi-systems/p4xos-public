#!/usr/bin/python

import os
import sys
import json
import logging
import argparse
import ConfigParser
from scapy.all import *
from interfaces import all_interfaces
from threading import Condition

THIS_DIR=os.path.dirname(os.path.realpath(__file__))
sys.path.append(THIS_DIR)
from paxoscore.learner import Learner

class SimpleDatabase(object):
    """
    Simple database backend
    """
    def __init__(self):
        self.current = 1
        self.cond = Condition()
        self.db = {}

    def execute(self, inst, cmd, d):
        with self.cond:
            while inst > self.current:
                self.cond.wait()
            if cmd['action'] == 'put':
                k = cmd['key'][0]
                v = cmd['value'][0]
                self.db[k] = v
                self.current += 1
                self.cond.notifyAll()
                d.callback("Success\n")
            if cmd['action'] == 'get':
                k = cmd['key'][0]
                self.current += 1
                self.cond.notifyAll()
                try:
                    v = self.db.get(k)
                    d.callback('%s\n' % v)
                except KeyError as ex:
                    d.callback("None\n")

    def deliver(self, res, d):
        inst, cmd = res
        inst = int(inst)
        cmd  = json.loads(cmd)
        self.execute(inst, cmd, d)

def main():
    parser = argparse.ArgumentParser(description='Paxos Proposer.')
    parser.add_argument('--cfg', required=True)
    args = parser.parse_args()
    config = ConfigParser.ConfigParser()
    config.read(args.cfg)
    itfs = all_interfaces()
    itf_names = zip(*itfs)[0]
    learner = Learner(config, itf_names)
    dbserver = SimpleDatabase()
    learner.addDeliver(dbserver.deliver)
    try:
        learner.start()
    except (KeyboardInterrupt, SystemExit):
        learner.stop()
        sys.exit()

if __name__ == '__main__':
    main()
