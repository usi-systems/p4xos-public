
P4_PATH='/home/vagrant/p4factory'
PROG='paxos_l2_control'

python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "set_default_action smac mac_learn" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "set_default_action dmac broadcast" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "set_default_action mcast_src_pruning _nop" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "set_default_action round_tbl read_round" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "set_default_action drop_tbl _drop" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "set_default_action acceptor_tbl _drop" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "add_entry acceptor_tbl 1 handle_1a" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "add_entry acceptor_tbl 3 handle_2a" -c localhost:22222
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "add_entry mcast_src_pruning 5 _drop" -c localhost:22222
mgrphdl=`python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "mc_mgrp_create 1" -c localhost:22222 | awk '{print $NF;}'`
echo $mgrphdl > mgrp.hdl
l1hdl=`python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "mc_node_create 0 30 -1" -c localhost:22222 | awk '{print $NF;}'`
echo $l1hdl > l1.hdl
python $P4_PATH/cli/pd_cli.py -p $PROG -i p4_pd_rpc.$PROG -s ../tests/pd_thrift:$P4_PATH/testutils -m "mc_associate_node $mgrphdl $l1hdl" -c localhost:22222
