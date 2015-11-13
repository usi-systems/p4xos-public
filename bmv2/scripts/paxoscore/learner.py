#!/usr/bin/python

from math import ceil

class PaxosMessage(object):
    def __init__(self, nid, inst, crnd, vrnd, value):
        self.nid = nid
        self.inst = inst
        self.crnd = crnd
        self.vrnd = vrnd
        self.val = value

class Learner(object):
    def __init__(self, config, num_replica):
        self.logs = {}
        self.states = {}
        self.majority = ceil((num_replica + 1) / 2)
        self.config = config


    class LearnerState(object):
        def __init__(self, crnd):
            self.crnd = crnd
            self.nids = set()
            self.val = None
            self.finished = False


    def handle_p2b(self, msg):
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
