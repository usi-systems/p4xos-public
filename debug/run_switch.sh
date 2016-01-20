#!/bin/bash

PROG="paxos"

source env.sh
set -m
$P4C_BM_SCRIPT ../p4src/$PROG.p4 --json $PROG.json
sudo echo "sudo" > /dev/null
sudo PYTHONPATH=$PYTHONPATH:$BMV2_PATH/mininet/ python topo.py \
    --behavioral-exe $BMV2_PATH/targets/simple_switch/simple_switch \
    --json $PROG.json \
    --cli $CLI_PATH