#!/usr/bin/env python

from scapy.all import *
import sys
import argparse
import struct
import os
from subprocess import Popen, PIPE


def send_command(args, addr, port):
    cmd = [args.cli, args.json, args.thrift_port]
    r1 = 'table_add dmac_table forward %s => %d' % (addr, port)
    r2 = 'table_add smac_table _nop %s =>' % addr
    rules = [r1, r2]
    for rule in rules:
        p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        output, err = p.communicate(rule)
        if output:
            print output
        if err:
            print err

def handle(x, args):
    eth = x['Ethernet']
    if eth.type == 0x800:
        x.show()
        ip = x['IP']
        if ip.len == 21:
            in_port = ord(x['Raw'].load)
            print "src %s, in_port %d" % (eth.src, in_port)
            send_command(args, eth.src, in_port)

def server(args):
    sniff(iface = args.interface, prn = lambda x: handle(x, args))

def main():
    parser = argparse.ArgumentParser(description='receiver and sender to test P4 program')
    parser.add_argument("-i", "--interface", default='veth1', help="bind to specified interface")
    args = parser.parse_args()
    args.cli = "/home/vagrant/bmv2/targets/simple_switch/sswitch_CLI"
    args.json = "paxos.json"
    args.thrift_port = "22222"
    server(args)

if __name__=='__main__':
    main()