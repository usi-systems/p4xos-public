#!/usr/bin/python

import struct
import logging
from math import ceil
from scapy.all import *
from twisted.internet import defer
from threading import Thread
import json
import netifaces

logging.basicConfig(level=logging.DEBUG,format='%(message)s')
VALUE_SIZE = 64
PHASE_1A = 1
PHASE_1B = 2
PHASE_2A = 3
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
        self.proposerState = {}
        self.majority = ceil((num_acceptors + 1) / 2)


    class ProposerState(object):
        """
        The state of learner of a particular instance.
        """
        def __init__(self, crnd):
            self.crnd = crnd
            self.nids = set()
            self.hval = None
            self.hvrnd = 0
            self.finished = False

    class LearnerState(object):
        """
        The state of learner of a particular instance.
        """
        def __init__(self, crnd):
            self.crnd = crnd
            self.nids = set()
            self.val = None
            self.finished = False


    def handle_p1b(self, msg):
        """handle 1a message and return decision if existing a majority.
        Otherwise return None"""
        res = None
        state = self.proposerState.get(msg.inst)  
        if state is None:
            state = self.ProposerState(msg.crnd)
        if not state.finished:
            if state.crnd == msg.crnd:
                if msg.nid not in state.nids:
                    state.nids.add(msg.nid)
                    if state.hvrnd <= msg.vrnd:
                        state.hval = msg.val

                    if len(state.nids) >= self.majority:
                        state.finished = True
                        res = PaxosMessage(10, msg.inst, state.crnd, state.hvrnd, state.hval)                        
                    self.proposerState[msg.inst] = state
        return res


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
    def __init__(self, num_acceptors, learner_addr, learner_port):
        """
        Initialize a learner with the number of acceptors, maximum number of requests,
        and the running duration.
        """
        self.learner = PaxosLearner(num_acceptors)
        self.learner_addr = learner_addr
        self.learner_port = learner_port
        self.minUncommitedIndex = 1
        self.maxInstance = 1

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

    def make_paxos(self, typ, i, rnd, vrnd, val):
        request_id = 10
        acceptor_id = 10
        values = (typ, i, rnd, vrnd, acceptor_id, request_id, val)
        packer = struct.Struct('!' + 'B H B B Q B {0}s'.format(VALUE_SIZE-1))
        packed_data = packer.pack(*values)
        return packed_data

    def sendMsg(self, msg, dst, dport):
        """
        This method sends the reply from application server to the origin of the request.
        """
        for itf in netifaces.interfaces():
            ether = Ether(src='00:04:00:00:00:01', dst='01:00:5e:03:1d:47')
            pkt_header = ether/IP(dst=dst)/UDP(sport=12345, dport=dport)
            sendp(pkt_header/msg, iface=itf, verbose=False)

    def retryInstance(self, inst):
        msg1a = self.make_paxos(PHASE_1A, inst, 1, 0, '')
        self.sendMsg(msg1a, self.learner_addr, self.learner_port)

    def deliverInstance(self, inst):
        try:
            d = defer.Deferred()
            if inst == self.minUncommitedIndex:
                cmd = self.learner.logs[inst]
                cmd_in_dict = json.loads(cmd)
                print "%d %s" % (inst, cmd_in_dict)
                self.deliver(cmd_in_dict, d)
                self.minUncommitedIndex += 1
                print "minUncommitedIndex: %d" % self.minUncommitedIndex
                if inst < self.maxInstance:
                    print "Try deliver next instance %d" % (inst + 1)
                    self.deliverInstance(inst + 1)
                return d
            elif inst > self.minUncommitedIndex:
                print "Try deliver instance %d" % (inst - 1)
                return self.deliverInstance(inst - 1)
        except KeyError as keyerr:
            print "Log has a gap at index %d" % inst
            self.retryInstance(inst)
            d.callback("Retry")
            return d
        except:
            print "Unexpected error:", sys.exc_info()[0]
            raise



    def handle_pkt(self, pkt):
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
            msg = PaxosMessage(acceptor_id, inst, rnd, vrnd, value)
            if typ == PHASE_2B:
                res = self.learner.handle_p2b(msg)
                if res is not None:
                    inst = int(res[0])
                    if self.maxInstance < inst:
                        self.maxInstance = inst
                    d = self.deliverInstance(inst)
                    d.addCallback(self.respond, req_id, pkt[IP].src,
                        pkt[UDP].dport, pkt[UDP].sport)
            elif typ == PHASE_1B:
                res = self.learner.handle_p1b(msg)
                if res is not None:
                    msg2a = self.make_paxos(PHASE_2A, res.inst, res.crnd, res.vrnd, res.val)
                    self.sendMsg(msg2a, self.learner_addr, self.learner_port)
        except IndexError as ex:
            logging.error(ex)

    def start(self, count, timeout):
        """
        Start a learner by sniffing on all learner's interfaces.
        """
        logging.debug("| %10s | %4s |  %2s | %2s | %4s | %s |" % \
             ("type", "inst", "pr", "ar", "val", "payload"))
        try:
            if timeout > 0:
                sniff(count=count, timeout=timeout, filter="udp && dst port 34952",
                  prn = lambda x: self.handle_pkt(x))
            else:
                sniff(count=count, filter="udp && dst port 34952",
                  prn = lambda x: self.handle_pkt(x))
        except Exception as e:
            logging.error(e)
        return

    def stop(self):
        """
        Stop sniffing on the learner's interfaces. Not implemented yet
        """
        pass
