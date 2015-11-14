#!/usr/bin/python
from scapy.all import *

import struct
import sys
import os

import struct
import array
from interfaces import all_interfaces
from threading import Thread, Timer, Condition
from math import ceil
import logging
import ConfigParser
import argparse
import json
import binascii
from twisted.internet import defer

THIS_DIR=os.path.dirname(os.path.realpath(__file__))
sys.path.append(THIS_DIR)

from paxoscore.learner import Learner, PaxosMessage

logging.basicConfig(level=logging.DEBUG,format='%(message)s')
VALUE_SIZE = 64
PHASE_2B = 4

THIS_DIR=os.path.dirname(os.path.realpath(__file__))

class LServer(object):
    def __init__(self, learner, config, itfs):
        self.learner = learner
        self.config = config
        self.itfs = itfs
        self.threads = []
        self.current = 1
        self.cond = Condition()
        self.db = {}

    def respond(self, result, req_id, dst, sport, dport):
        packer = struct.Struct('>' + 'B {0}s'.format(VALUE_SIZE))
        packed_data = packer.pack(*(req_id, str(result)))
        pkt_header = IP(dst=dst)/UDP(sport=sport, dport=dport)
        send(pkt_header/packed_data, verbose=False)

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

    def handle_pkt(self, pkt, itf):
        paxos_type = { 1: "prepare", 2: "promise", 3: "accept", 4: "accepted" }
        try:
            if pkt['IP'].proto != 0x11:
                return
            datagram = pkt['Raw'].load
            fmt = '>' + 'B H B B B {0}s'.format(VALUE_SIZE - 1)
            packer = struct.Struct(fmt)
            packed_size = struct.calcsize(fmt)
            unpacked_data = packer.unpack(datagram[:packed_size])
            typ, inst, rnd, vrnd, req_id, value = unpacked_data
            value = value.rstrip('\t\r\n\0')
            logging.debug("| %10s | %4d |  %02x | %02x | %d | %64s |" % \
                    (paxos_type[typ], inst, rnd, vrnd, req_id, value))
            if typ == PHASE_2B:
                msg = PaxosMessage(itf, inst, rnd, vrnd, value)
                res = self.learner.handle_p2b(msg)
                if res is not None:
                    inst, cmd = res
                    inst = int(inst)
                    cmd  = json.loads(cmd)
                    d = defer.Deferred()
                    d.addCallback(self.respond, req_id, pkt[IP].src,
                                    pkt[UDP].dport, pkt[UDP].sport)
                    self.execute(inst, cmd, d)
 
        except IndexError as ex:
            logging.error(ex)

    def worker(self, itf):
        "thread worker function"
        count = self.config.getint('instance', 'count')
        t = self.config.getint('timeout', 'second')
        try:
            if t > 0:
                sniff(iface=itf, count=count, timeout=t, filter="udp && dst port 34952",
                  prn = lambda x: self.handle_pkt(x, itf))
            else:
                sniff(iface=itf, count=count, filter="udp && dst port 34952",
                  prn = lambda x: self.handle_pkt(x, itf))
        except Exception as e:
            logging.exception('{0}, interface {1}'.format(e, itf))
        return

    def start(self):
        logging.debug("| %10s | %4s |  %2s | %2s | %4s | %s |" % \
             ("type", "inst", "pr", "ar", "val", "payload"))
        for i in self.itfs:
            t = Thread(target=self.worker, args=(i,))
            self.threads.append(t)
            t.start()

    def stop(self):
        pass

def main():
    parser = argparse.ArgumentParser(description='Paxos Proposer.')
    parser.add_argument('--cfg', required=True)
    args = parser.parse_args()
    config = ConfigParser.ConfigParser()
    config.read(args.cfg)
    itfs = all_interfaces()
    itf_names = zip(*itfs)[0]
    learner = Learner(config, len(itf_names))
    lserver = LServer(learner, config, itf_names)
    try:
        lserver.start()
    except (KeyboardInterrupt, SystemExit):
        lserver.stop()
        sys.exit()

if __name__ == '__main__':
    main()
