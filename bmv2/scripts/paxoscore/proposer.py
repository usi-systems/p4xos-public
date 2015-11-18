#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor, defer
from twisted.web.server import Site, NOT_DONE_YET
import logging
import struct

logging.basicConfig(level=logging.DEBUG,format='%(message)s')

VALUE_SIZE = 64
PHASE_2A = 3

class Proposer(DatagramProtocol):
    """
    Proposer class implements a Paxos proposer which sends requests and wait 
    asynchronously for responses.
    """
    def __init__(self, config, proposer_id):
        """
        Initialize a Proposer with a configuration of learner address and port.
        The proposer is also configured with a port for receiving UDP packets.
        """
        self.dst = (config.get('learner', 'addr'), \
                    config.getint('learner', 'port'))
        self.rnd = proposer_id
        self.req_id = 0
        self.defers = {}

    def submit(self, msg):
        """
        Submit a request with an associated request id. The request id is used
        to lookup the original request when receiving a response.
        """
        self.req_id += 1
        values = (PHASE_2A, 0, self.rnd, self.rnd, 0, self.req_id, msg)
        packer = struct.Struct('>' + 'B H B B Q B {0}s'.format(VALUE_SIZE-1))
        packed_data = packer.pack(*values)
        self.transport.write(packed_data, self.dst)
        self.defers[self.req_id] = defer.Deferred()
        return self.defers[self.req_id]

    def datagramReceived(self, datagram, address):
        """
        Receive response from Paxos Learners, match the response with the original 
        request and pass it to the application handler.
        """
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
