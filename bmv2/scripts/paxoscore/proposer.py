#!/usr/bin/env python
"""Paxos Client"""
__author__ = "Tu Dang"

import string
import struct
import os

THIS_DIR=os.path.dirname(os.path.realpath(__file__))

class Proposer(object):
    def __init__(self, config, conn, proposer_id):
        self.config = config
        self.conn = conn
        self.rnd = proposer_id

    def submitRandomValue(self):
        msg = ''.join(random.SystemRandom().choice(string.ascii_uppercase + \
                string.digits) for _ in range(VALUE_SIZE))
        self.submit(msg)

    def submit(self, msg):
        self.conn.send(msg)

    def deliver(self, msg):
        return msg
