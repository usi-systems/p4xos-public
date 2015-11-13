#!/usr/bin/python
from scapy.all import *

import struct
import sys
import os

import struct
import array
from interfaces import all_interfaces
from threading import Thread, Timer
from math import ceil
import logging
import ConfigParser
import argparse

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
            logging.debug("| %10s | %4d |  %02x | %02x | %d | %64s |" % \
                    (paxos_type[typ], inst, rnd, vrnd, req_id, value))
            if typ == PHASE_2B:
                msg = PaxosMessage(itf, inst, rnd, vrnd, value)
                res = self.learner.handle_p2b(msg)
                if res is not None:
                    packer = struct.Struct('>' + 'B {0}s'.format(VALUE_SIZE))
                    packed_data = packer.pack(*(req_id, res[1]))
                    pkt_header = IP(dst=pkt[IP].src)/UDP(sport=pkt[UDP].dport,
                                    dport=pkt[UDP].sport)
                    send(pkt_header/packed_data, verbose=False)
 
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
            logging.error('{0}, interface {1}'.format(e, itf))
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
