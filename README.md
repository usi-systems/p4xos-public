# Setup a Virtual Machine

We provide a VM to run p4paxos demo here: [usi-systems/sdn-coure-vm](https://github.com/usi-systems/sdn-course-vm)

Please follow theses [instructions](https://github.com/usi-systems/sdn-course-vm/blob/master/README.md) to create a new virtual machine.


# Demo

After connecting to the VM, change to the **p4paxos/bmv2** directory, and start the demo

```
cd p4paxos/bmv2
sudo ./run_demo.sh
```

From the mininet prompt

```
mininet> xterm h1 h2
```

## Paxos Learner

Run learner agent on h2 (or h3)

```
./scripts/learnerAgent.py --cfg scripts/paxos.cfg
```

## Paxos Proposer

Run proposer agent on h1 (or h4)

```
./scripts/proposerAgent.py --cfg scripts/paxos.cfg
```

## Config

By default, the demo only lasts for 5 seconds, or after the learner receives 10 Paxos messages.

You can change this by modifying the *count* variable in the *instance* section or *second* variable in *timeout* section of the *bmv2/scripts/paxos.cfg* configuration file.
If timeout is set to 0, the learner will exit after receiving 10 Paxos messages.

```
[instance]
count=10

[timeout]
second=5
```
