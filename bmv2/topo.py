#!/usr/bin/python

# Copyright 2013-present Barefoot Networks, Inc. 
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from mininet.link import TCLink

from p4_mininet import P4Switch, P4Host

import argparse
from time import sleep
import os
import subprocess
from subprocess import PIPE

_THIS_DIR = os.path.dirname(os.path.realpath(__file__))
_THRIFT_BASE_PORT = 22222

parser = argparse.ArgumentParser(description='Mininet demo')
parser.add_argument('--behavioral-exe', help='Path to behavioral executable',
                    type=str, action="store", required=True)
parser.add_argument('--acceptor', help='Path to acceptor JSON config file',
                    type=str, action="store", required=True)
parser.add_argument('--coordinator', help='Path to coordinator JSON config file',
                    type=str, action="store", required=True)
parser.add_argument('--cli', help='Path to BM CLI',
                    type=str, action="store", required=True)
parser.add_argument('--start-server', help='Start Paxos httpServer and backends',
                    action="store_true", default=False)

args = parser.parse_args()

class MyTopo(Topo):
    def __init__(self, sw_path, acceptor, coordinator,  **opts):
        # Initialize topology and default options
        Topo.__init__(self, **opts)

        s1 = self.addSwitch('s1',
                      sw_path = args.behavioral_exe,
                      json_path = coordinator,
                      thrift_port = _THRIFT_BASE_PORT + 1,
                      pcap_dump = False,
                      device_id = 1)

        h1 = self.addHost('h1')
        h4 = self.addHost('h4')
        self.addLink(h1, s1)
        self.addLink(h4, s1)
        switches = []
        hosts = []
        for i in [2, 3, 4]:
            switches.append(self.addSwitch('s%d' % (i),
                                    sw_path = sw_path,
                                    json_path = acceptor,
                                    thrift_port = _THRIFT_BASE_PORT + i,
                                    pcap_dump = False,
                                    device_id = i))
        
        for h in [2,3]:
            hosts.append(self.addHost('h%d' % (h)))

        for i, s in enumerate(switches):
            for j, h in enumerate(hosts):
                self.addLink(h, s, intfName1='eth{0}'.format(i+1),
                            params1={'ip': '10.0.{0}.{1}/8'.format(i+1, j+2)}
                            )
            self.addLink(s, s1)


def main():
    topo = MyTopo(args.behavioral_exe,
                  args.acceptor, args.coordinator)

    net = Mininet(topo = topo,
                  host = P4Host,
                  switch = P4Switch,
                  controller = None )

    net.start()

    for n in [1, 2, 3, 4]: 
        h = net.get('h%d' % n)
        for off in ["rx", "tx", "sg"]:
            cmd = "/sbin/ethtool --offload eth0 %s off" % off
            print cmd
            h.cmd(cmd)
        print "disable ipv6"
        h.cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=1")
        h.cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=1")
        h.cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=1")
        h.cmd("sysctl -w net.ipv4.tcp_congestion_control=reno")
        h.cmd("iptables -I OUTPUT -p icmp --icmp-type destination-unreachable -j DROP")
        print "add mutlicast route"
        h.cmd("route add -net 224.0.0.0 netmask 224.0.0.0 eth0")

    sleep(2)

    for i in [2, 3, 4]:
        cmd = [args.cli, args.acceptor, str(_THRIFT_BASE_PORT + i)]
        with open("acceptor_commands.txt", "r") as f:
            print " ".join(cmd)
            try:
                output = subprocess.check_output(cmd, stdin = f)
                print output
            except subprocess.CalledProcessError as e:
                print e
                print e.output

    cmd = [args.cli, args.coordinator, str(_THRIFT_BASE_PORT + 1)]
    with open("coordinator_commands.txt", "r") as f:
        print " ".join(cmd)
        try:
            output = subprocess.check_output(cmd, stdin = f)
            print output
        except subprocess.CalledProcessError as e:
            print e
            print e.output

    for i in [2, 3, 4]:
        cmd = [args.cli, args.acceptor, str(_THRIFT_BASE_PORT + i)]
        rule = 'register_write datapath_id 0 %d' % (i-1)
        p = subprocess.Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        out, err = p.communicate(rule)
        if out:
            print out
        if err:
            print err

    if args.start_server:
        h1 = net.get('h1')
        h1.cmd("python scripts/httpServer.py --cfg scripts/paxos.cfg &")
        h2 = net.get('h2')
        h2.cmd("python scripts/backend.py --cfg scripts/paxos.cfg &")
        h3 = net.get('h3')
        h3.cmd("python scripts/backend.py --cfg scripts/paxos.cfg &")

    print "Ready !"

    CLI( net )
    net.stop()

if __name__ == '__main__':
    setLogLevel( 'info' )
    main()
