#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

import string
import struct
import random

VALUE_SIZE = 64

class Proposer(object):
    def __init__(self, conn, proposer_id):
        self.conn = conn
        self.rnd = proposer_id

    def submitRandomValue(self):
        msg = ''.join(random.SystemRandom().choice(string.ascii_uppercase + \
                string.digits) for _ in range(VALUE_SIZE))
        self.submit(msg)

    def submit(self, msg):
        self.conn.send(self.rnd, msg)

    def deliver(self, msg):
        print msg
        return msg
