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

THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

source $THIS_DIR/env.sh

P4_SRC=../p4src
ACCEPTOR="$P4_SRC/acceptor"
COORDINATOR="$P4_SRC/coordinator"
LEARNER="$P4_SRC/learner"

P4C_BM_SCRIPT=$P4C_BM_PATH/p4c_bm/__main__.py
SWITCH_PATH=$BMV2_PATH/targets/simple_switch/simple_switch
CLI_PATH=$BMV2_PATH/targets/simple_switch/sswitch_CLI

$P4C_BM_SCRIPT $ACCEPTOR.p4 --json $ACCEPTOR.json
$P4C_BM_SCRIPT $COORDINATOR.p4 --json $COORDINATOR.json
$P4C_BM_SCRIPT $LEARNER.p4 --json $LEARNER.json

sudo PYTHONPATH=$PYTHONPATH:$BMV2_PATH/mininet/ python topo.py \
    --behavioral-exe $BMV2_PATH/targets/simple_switch/simple_switch \
    --acceptor $ACCEPTOR.json \
    --coordinator $LEARNER.json \
    --cli $CLI_PATH \
    --start-server
