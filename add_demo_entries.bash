python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "set_default_action smac mac_learn" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "set_default_action dmac broadcast" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "set_default_action mcast_src_pruning _nop" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "set_default_action round_tbl read_round" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "set_default_action drop_tbl _drop" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "add_entry accept_tbl 1 promise" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "add_entry accept_tbl 3 accept" -c localhost:22222
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "add_entry mcast_src_pruning 5 _drop" -c localhost:22222
mgrphdl=`python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "mc_mgrp_create 1" -c localhost:22222 | awk '{print $NF;}'`
echo $mgrphdl > mgrp.hdl
l1hdl=`python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "mc_node_create 0 30 -1" -c localhost:22222 | awk '{print $NF;}'`
echo $l1hdl > l1.hdl
python ../../cli/pd_cli.py -p l2_switch -i p4_pd_rpc.l2_switch -s $PWD/tests/pd_thrift:$PWD/../../testutils -m "mc_associate_node $mgrphdl $l1hdl" -c localhost:22222
