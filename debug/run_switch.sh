#!/bin/bash

PROG="paxos"

source env.sh
set -m
$P4C_BM_SCRIPT ../p4src/$PROG.p4 --json $PROG.json
sudo echo "sudo" > /dev/null
sudo $BMV2_PATH/targets/simple_switch/simple_switch $PROG.json \
    -i 0@veth0 -i 1@veth2 -i 2@veth4 -i 3@veth6 -i 4@veth8 \
    --nanolog ipc:///tmp/bm-0-log.ipc &
sleep 2
$CLI_PATH $PROG.json < commands.txt
echo "READY!!!"
fg