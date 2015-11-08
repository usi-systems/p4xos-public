#!/usr/bin/env python

from subprocess import Popen, PIPE

commands = """sudo mn -c
sudo killall behavioral-model
redis-cli FLUSHALL"""

for cmd in commands.splitlines():
    sp = Popen(cmd.split(), stdout=PIPE) 
    output = sp.communicate()[0]
    print output
