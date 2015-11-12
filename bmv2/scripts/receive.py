#!/usr/bin/python
from scapy.all import *

import struct
import sys
import os

import socket
import fcntl
import struct
import array
from interfaces import all_interfaces
from threading import Thread, Timer
from math import ceil
import logging
import ConfigParser

logging.basicConfig(level=logging.DEBUG,
                    format='[%(levelname)s] (%(threadName)-10s) %(message)s',
                    )
VALUE_SIZE = 64
PHASE_2B = 4

THIS_DIR=os.path.dirname(os.path.realpath(__file__))

class Learner(object):
    def __init__(self, itfs, config):
        self.logs = {}
        self.states = {}
        self.itfs = itfs
        self.threads = []
        self.majority = ceil((len(itfs) + 1) / 2)
        self.config = config


    class LearnerState(object):
        def __init__(self, crnd):
            self.crnd = crnd
            self.nids = set()
            self.val = None
            self.finished = False

    class PaxosMessage(object):
        def __init__(self, nid, inst, crnd, vrnd, value):
            self.nid = nid
            self.inst = inst
            self.crnd = crnd
            self.vrnd = vrnd
            self.val = value

    def handle_accepted(self, msg):
        res = None
        state = self.states.get(msg.inst)
        if state is None:
            state = self.LearnerState(msg.crnd)

        if not state.finished:
            if state.crnd < msg.crnd:
                state = LearnerState(msg.crnd)
            if state.crnd == msg.crnd:
                if msg.nid not in state.nids:
                    state.nids.add(msg.nid)
                    if state.val is None:
                        state.val = msg.val

                    if len(state.nids) >= self.majority:
                        state.finished = True
                        self.logs[msg.inst] = state.val
                        res = (msg.inst, state.val)

                    self.states[msg.inst] = state
        return res


    def handle_pkt(self, pkt, itf):
        paxos_type = { 1: "prepare", 2: "promise", 3: "accept", 4: "accepted" }
        try:
            if pkt['IP'].proto != 0x11:
                return
            datagram = pkt['Raw'].load
            fmt = '>' + 'B H B B {0}s'.format(VALUE_SIZE)
            packer = struct.Struct(fmt)
            packed_size = struct.calcsize(fmt)
            unpacked_data = packer.unpack(datagram[:packed_size])
            remaining_payload = datagram[packed_size:]
            typ, inst, rnd, vrnd, value = unpacked_data
            logging.debug("| %10s | %4d |  %02x | %02x | %64s | %s |" % \
                    (paxos_type[typ], inst, rnd, vrnd, value, remaining_payload))
            if typ == PHASE_2B:
                msg = self.PaxosMessage(itf, inst, rnd, vrnd, value)
                res = self.handle_accepted(msg)
                if res is not None:
                    logging.debug(res)
 
        except IndexError as ex:
            logging.error(ex)

    def worker(self, itf):
        "thread worker function"
        count = self.config.getint('instance', 'count')
        t = self.config.getint('timeout', 'second')
        sniff(iface=itf, count=count, timeout=t, filter="udp && dst port 34952",
              prn = lambda x: self.handle_pkt(x, itf))
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
    config = ConfigParser.ConfigParser()
    config.read('%s/paxos.cfg' % THIS_DIR)
    itfs = all_interfaces()
    itf_names = zip(*itfs)[0]
    learner = Learner(itf_names, config)
    try:
        learner.start()
    except (KeyboardInterrupt, SystemExit):
        learner.stop()
        sys.exit()

if __name__ == '__main__':
    main()
