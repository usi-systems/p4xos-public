#!/usr/bin/env python

from scapy.all import *
import sys
import argparse
import struct
import os
import subprocess


def send_command(args, addr, port):
    cmd = [args.cli, args.json, args.thrift_port]
    with open("tmp.txt", "w+") as f:
        f.write('table_add dmac_table forward %s => %d\n' % (addr, port))
        f.write('table_add smac_table _nop %s =>\n' % addr)
        f.seek(0)
        print " ".join(cmd)
        try:
            output = subprocess.check_output(cmd, stdin = f)
            print output
        except subprocess.CalledProcessError as e:
            print e
            print e.output
    os.remove('tmp.txt')

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