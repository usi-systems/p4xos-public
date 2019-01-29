#!/bin/bash

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

# reference to bmv2 and p4c-bm2
##########################
BMV2_PATH=~/bmv2
P4C_BM_PATH=~/p4c-bmv2
##########################

THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

P4C_BM_SCRIPT=$P4C_BM_PATH/p4c_bm/__main__.py
SWITCH_PATH=$BMV2_PATH/targets/simple_switch/simple_switch
CLI_PATH=$BMV2_PATH/targets/simple_switch/sswitch_CLI

$P4C_BM_SCRIPT ../p4src/paxos_coordinator.p4 --json paxos_coordinator.json
$P4C_BM_SCRIPT ../p4src/paxos_acceptor.p4 --json paxos_acceptor.json
$P4C_BM_SCRIPT ../p4src/paxos_learner.p4 --json paxos_learner.json

sudo PYTHONPATH=$PYTHONPATH:$BMV2_PATH/mininet/ python topo.py \
    --behavioral-exe $BMV2_PATH/targets/simple_switch/simple_switch \
    --acceptor paxos_acceptor.json \
    --coordinator paxos_coordinator.json \
    --cli $CLI_PATH \
    --start-server
