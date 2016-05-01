ovs-vsctl del-br br0
ovs-vsctl add-br br0 -- set bridge br0 datapath_type=pica8
ovs-vsctl add-port br0 te-1/1/2   -- set interface te-1/1/2 type=pica8
ovs-vsctl add-port br0 te-1/1/4   -- set interface te-1/1/4 type=pica8
ovs-vsctl add-port br0 te-1/1/9   -- set interface te-1/1/9 type=pica8
ovs-vsctl add-port br0 te-1/1/14  -- set interface te-1/1/14 type=pica8
ovs-vsctl add-port br0 te-1/1/16  -- set interface te-1/1/16 type=pica8
ovs-vsctl add-port br0 te-1/1/24  -- set interface te-1/1/24 type=pica8
ovs-ofctl add-flow br0 priority=10,in_port=9,action=output:14
ovs-ofctl add-flow br0 priority=10,in_port=16,action=output:4
ovs-ofctl add-flow br0 priority=10,in_port=2,action=output:24
ovs-ofctl add-flow br0 priority=20,ip,nw_dst=192.168.4.95,action=output:9
ovs-ofctl add-flow br0 priority=20,ip,nw_dst=192.168.4.98,action=output:24