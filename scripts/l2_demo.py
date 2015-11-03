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

import sys
sys.path.append('/home/vagrant/p4factory/mininet')

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from mininet.node import Switch, Host
from mininet.link import Intf

from p4_mininet import P4Switch

import argparse
from time import sleep
parser = argparse.ArgumentParser(description='Mininet demo')
parser.add_argument('--behavioral-exe', help='Path to behavioral executable',
                    type=str, action="store", required=True)
parser.add_argument('--thrift-port', help='Thrift server port for table updates',
                    type=int, action="store", default=22222)
parser.add_argument('--num-hosts', help='Number of hosts to connect to switch',
                    type=int, action="store", default=2)

args = parser.parse_args()


class P4Host(Host):
    def config(self, **params):
        r = super(Host, self).config(**params)

        self.defaultIntf().rename("eth0")

        for off in ["rx", "tx", "sg"]:
            cmd = "/sbin/ethtool --offload eth0 %s off" % off
            self.cmd(cmd)

        # disable IPv6
        self.cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=1")
        self.cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=1")
        self.cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=1")

        return r

    def describe(self):
        print "**********"
        print self.name
        print "default interface: %s\t%s\t%s" %(
            self.defaultIntf().name,
            self.defaultIntf().IP(),
            self.defaultIntf().MAC()
        )
        print "**********"

class NetPaxosTopo(Topo):
    "Multiple switches connected in tree topo"
    def __init__(self, sw_path, thrift_port, n, **opts):
        # Initialize topology and default options
        Topo.__init__(self, **opts)

        switches = []
        for i in range (n):
            switch = self.addSwitch('s{0}'.format(i+1),
                                sw_path = sw_path,
                                thrift_port = thrift_port,
                                pcap_dump = True)
            switches.append(switch)

        for h in range(1, n+1):
            host = self.addHost('h%d' % h,
				ip = '10.0.0.%d' % h,
                                mac = '00:04:00:00:00:%02x' %h)
            for i, switch in enumerate(switches):
                self.addLink(host, switch, intfName1='eth{0}'.format(i+1),
                            params1={'ip': '10.0.{0}.{1}/8'.format(i+1, h)})

class SingleTopo(Topo):
    "Multiple switches connected in tree topo"
    def __init__(self, sw_path, thrift_port, n, **opts):
        # Initialize topology and default options
        Topo.__init__(self, **opts)

        switch = self.addSwitch('s1',
                                sw_path = sw_path,
                                thrift_port = thrift_port,
                                pcap_dump = True)

        for h in range(1,n+1):
            host = self.addHost('h%d' % h)
            self.addLink(host, switch)

def main():
    num_hosts = args.num_hosts

    #topo = NetPaxosTopo(args.behavioral_exe,
    topo = SingleTopo(args.behavioral_exe,
                            args.thrift_port,
                            num_hosts
    )
    net = Mininet(topo = topo,
                  host = P4Host,
                  switch = P4Switch,
                  controller = None )
    s1 = net.get('s1')
    Intf('eth0', node=s1)
    net.start()


    sw_mac = ["00:aa:bb:00:00:%02x" % n for n in xrange(num_hosts)]

    for n in xrange(num_hosts):
        h = net.get('h%d' % (n + 1))
        h.describe()


    print "Ready !"

    CLI( net )
    net.stop()

if __name__ == '__main__':
    setLogLevel( 'info' )
    main()
