#!/usr/bin/python

import struct
import logging
from math import ceil
from scapy.all import *
from twisted.internet import defer
from threading import Thread

logging.basicConfig(level=logging.DEBUG,format='%(message)s')
VALUE_SIZE = 64
PHASE_2B = 4

class PaxosMessage(object):
    """
    PaxosMessage class defines the structure of a Paxos message
    """
    def __init__(self, nid, inst, crnd, vrnd, value):
        """
        Initialize a Paxos message with:
        nid : node-id
        inst: paxos instance
        crnd: proposer round
        vrnd: accepted round
        value: accepted value
        """
        self.nid = nid
        self.inst = inst
        self.crnd = crnd
        self.vrnd = vrnd
        self.val = value

class PaxosLearner(object):
    """
    PaxosLearner acts as Paxos learner that learns a decision from a majority
    of acceptors. 
    """
    def __init__(self, num_acceptors):
        """
        Initialize a learner with a the number of acceptors to decide the 
        quorum size.
        """
        self.logs = {}
        self.states = {}
        self.majority = ceil((num_acceptors + 1) / 2)


    class LearnerState(object):
        """
        The state of learner of a particular instance.
        """
        def __init__(self, crnd):
            self.crnd = crnd
            self.nids = set()
            self.val = None
            self.finished = False


    def handle_p2b(self, msg):
        """handle 2a message and return decision if existing a majority.
        Otherwise return None"""
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

class Learner(object):
    """
    A learner instance provides the ordering of requests to the overlay application.
    If a decision has been made, the learner delivers that decision to the application.
    """
    def __init__(self, config, itfs):
        """
        Initialize a learner with the set of network interfaces and a configuration
        file specifies the port that the learner will listen on and the address that 
        it will bind to.
        """
        self.learner = PaxosLearner(len(itfs))
        self.config = config
        self.itfs = itfs
        self.threads = []

    def respond(self, result, req_id, dst, sport, dport):
        """
        This method sends the reply from application server to the origin of the request.
        """
        packer = struct.Struct('>' + 'B {0}s'.format(VALUE_SIZE))
        packed_data = packer.pack(*(req_id, str(result)))
        pkt_header = IP(dst=dst)/UDP(sport=sport, dport=dport)
        send(pkt_header/packed_data, verbose=False)

    def addDeliver(self, deliver_cb):
        """
        This method allows the application server attach the request handler when such a
        request has been chosen for servicing.
        """
        self.deliver = deliver_cb

    def handle_pkt(self, pkt, itf):
        """
        This method handles the arrived packet, such as parsing, handing out the packet to
        Paxos learner module, and delivering the decision.
        """
        paxos_type = { 1: "prepare", 2: "promise", 3: "accept", 4: "accepted" }
        try:
            if pkt['IP'].proto != 0x11:
                return
            datagram = pkt['Raw'].load
            fmt = '>' + 'B H B B Q B {0}s'.format(VALUE_SIZE - 1)
            packer = struct.Struct(fmt)
            packed_size = struct.calcsize(fmt)
            unpacked_data = packer.unpack(datagram[:packed_size])
            typ, inst, rnd, vrnd, acceptor_id, req_id, value = unpacked_data
            value = value.rstrip('\t\r\n\0')
            logging.debug('acceptor_id: %d' % acceptor_id)
            #logging.debug("| %10s | %4d |  %02x | %02x | %d | %64s |" % \
            #        (paxos_type[typ], inst, rnd, vrnd, req_id, value))
            if typ == PHASE_2B:
                msg = PaxosMessage(itf, inst, rnd, vrnd, value)
                res = self.learner.handle_p2b(msg)
                if res is not None:
                    d = defer.Deferred()
                    self.deliver(res, d)
                    d.addCallback(self.respond, req_id, pkt[IP].src,
                        pkt[UDP].dport, pkt[UDP].sport)
        except IndexError as ex:
            logging.error(ex)

    def worker(self, itf):
        """
        Each worker is a thread that captures the packet on the interface passing in the argument
        """
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
        """
        Start a learner by sniffing on all learner's interfaces.
        """
        logging.debug("| %10s | %4s |  %2s | %2s | %4s | %s |" % \
             ("type", "inst", "pr", "ar", "val", "payload"))
        for i in self.itfs:
            t = Thread(target=self.worker, args=(i,))
            self.threads.append(t)
            t.start()

    def stop(self):
        """
        Stop sniffing on the learner's interfaces. Not implemented yet
        """
        pass
