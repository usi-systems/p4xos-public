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

    def submitRandomValue(self):
        msg = ''.join(random.SystemRandom().choice(string.ascii_uppercase + \
                string.digits) for _ in range(VALUE_SIZE))
        self.submit(msg)

    def submit(self, msg):
        values = (PHASE_2A, 0, self.rnd, self.rnd, msg)
        packer = struct.Struct('>' + 'B H B B {0}s'.format(VALUE_SIZE))
        packed_data = packer.pack(*values)
        self.conn.send(packed_data)

    def deliver(self, msg):
        print msg
        return msg
