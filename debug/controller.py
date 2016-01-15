#!/usr/bin/env python

from scapy.all import *
import sys
import argparse
import struct
import os
import subprocess

_THRIFT_BASE_PORT = 9090

def send_command(args, addr, port):
    cmd = [args.cli, args.json, args.thrift_port]
    with open("tmp.txt", "w+") as f:
        f.write('table_add dmac forward %s => %d' % (addr, port))
        f.seek(0)
        print " ".join(cmd)
        try:
            output = subprocess.check_output(cmd, stdin = f)
            # print output
        except subprocess.CalledProcessError as e:
            print e
            print e.output
    os.remove('tmp.txt')

def handle(x, args):
    # hexdump(x)
    x.show()
    eth = x['Ethernet']
    fmt = '>' + '14s B'
    packer = struct.Struct(fmt)
    packed_size = struct.calcsize(fmt)
    eth_bytes, in_port = packer.unpack(str(x)[:packed_size])
    send_command(args, eth.src, in_port)

def server(args):
    sniff(iface = args.interface, prn = lambda x: handle(x, args))

def main():
    parser = argparse.ArgumentParser(description='receiver and sender to test P4 program')
    parser.add_argument("-i", "--interface", default='veth1', help="bind to specified interface")
    args = parser.parse_args()
    args.cli = "/home/vagrant/bmv2/targets/simple_switch/sswitch_CLI"
    args.json = "paxos.json"
    args.thrift_port = "9090"
    server(args)

if __name__=='__main__':
    main()