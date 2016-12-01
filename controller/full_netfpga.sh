#!/bin/sh
PORT=$1
ovs-vsctl del-br br0
ovs-vsctl add-br br0 -- set bridge br0 datapath_type=pica8
# node95 FPGA
ovs-vsctl add-port br0 te-1/1/5 -- set interface te-1/1/5 type=pica8
# node96: FPGA
ovs-vsctl add-port br0 te-1/1/19 -- set interface te-1/1/19 type=pica8
# node97 FPGA
ovs-vsctl add-port br0 te-1/1/2 -- set interface te-1/1/2 type=pica8
# node98 FPGA
ovs-vsctl add-port br0 te-1/1/18 -- set interface te-1/1/18 type=pica8
# node95: udp socket NIC
ovs-vsctl add-port br0 te-1/1/9 -- set interface te-1/1/9 type=pica8
# node96: udp socket NIC
ovs-vsctl add-port br0 te-1/1/21 -- set interface te-1/1/21 type=pica8
# node 97: NIC
ovs-vsctl add-port br0 te-1/1/10 -- set interface te-1/1/10 type=pica8
# node98: udp socket
ovs-vsctl add-port br0 te-1/1/24 -- set interface te-1/1/24 type=pica8
# ip group (node95, node96, node97, node98)
ovs-ofctl add-group br0 group_id=1,type=all,bucket=output:9,bucket=output:10,bucket=output:21,bucket=output:24
# rules from application
ovs-ofctl add-flow br0 priority=20,ip,nw_dst=192.168.4.95,actions=output:9
ovs-ofctl add-flow br0 priority=20,ip,nw_dst=192.168.4.96,actions=output:21
ovs-ofctl add-flow br0 priority=20,ip,nw_dst=192.168.4.97,actions=output:10
ovs-ofctl add-flow br0 priority=20,ip,nw_dst=192.168.4.98,actions=output:24
ovs-ofctl add-flow br0 priority=20,arp,actions=group:1
#ovs-ofctl add-flow br0 priority=20,arp,actions=output:9,output:10,output:21,output:24
# multicast to acceptor group (node95, node96, node98)
ovs-ofctl add-group br0 group_id=2,type=all,bucket=output:5,bucket=output:19,bucket=output:18
# rules proxies to Coordinator
ovs-ofctl add-flow br0 priority=20,in_port=9,udp,nw_dst=224.3.29.73,udp_dst=34952,actions=output:2
ovs-ofctl add-flow br0 priority=20,in_port=21,udp,nw_dst=224.3.29.73,udp_dst=34952,actions=output:2
ovs-ofctl add-flow br0 priority=20,in_port=10,udp,nw_dst=224.3.29.73,udp_dst=34952,actions=output:2
ovs-ofctl add-flow br0 priority=20,in_port=24,udp,nw_dst=224.3.29.73,udp_dst=34952,actions=output:2
# rule Coordinator to acceptors
ovs-ofctl add-flow br0 priority=20,in_port=2,udp,nw_dst=224.3.29.73,udp_dst=34952,actions=group:2
# multicast to learner group (node97, node96, node98)
ovs-ofctl add-group br0 group_id=3,type=all,bucket=output:10,bucket=output:21,bucket=output:24
ovs-ofctl add-flow br0 priority=20,udp,nw_dst=224.3.29.73,udp_dst=34953,actions=group:3
