#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

import string
import struct
import random

VALUE_SIZE = 64
PHASE_2A = 3

class Proposer(object):
    def __init__(self, conn, proposer_id):
        self.conn = conn
        self.rnd = proposer_id
        self.req_id = 0

    def submitRandomValue(self):
        msg = ''.join(random.SystemRandom().choice(string.ascii_uppercase + \
                string.digits) for _ in range(VALUE_SIZE))
        self.submit(msg)

    def submit(self, msg):
        self.req_id += 1
        values = (PHASE_2A, 0, self.rnd, self.rnd, self.req_id, msg)
        packer = struct.Struct('>' + 'B H B B B {0}s'.format(VALUE_SIZE-1))
        packed_data = packer.pack(*values)
        return self.conn.send(self.req_id, packed_data)

    def deliver(self, msg):
        return msg
