# Build behavioral-model
Required: p4factory

```
cd ~/
cd p4factory/targets
git clone --recursive https://github.com/usi-systems/p4paxos.git
cd p4paxos.git 
make
```

# Demo

Change to the **scripts** directory,
```
cd scripts
```

Create P4 virtual interfaces
```
sudo ~/p4factory/tools/veth_setup.sh
```

In another terminal, run demo

```
sudo ./run_demo.sh
```

Populate tables
```
./add_demo_entries.bash
```

From mininet prompt

```
mininet> xterm h1 h2
```

You can either run UDP server or packet sniffer on h1.
The UDP server listens on the port and joins the multicast group 
specified in the config file **paxos.cfg**

## UDP server

Run server on h1

```
./route-add.sh
./server.py
```

## Scapy - packet sniffer

Run packet sniffer on h1

```
./receive.py eth0
```

## UDP Client

Run client on h2

```
./route-add.sh
./client.py
usage: client.py [-h] [--typ TYP] [--inst INST] [--rnd RND] [--vrnd VRND]
                 [--value VALUE]

Generate Paxos messages.

optional arguments:
  -h, --help     show this help message and exit
  --typ TYP
  --inst INST
  --rnd RND
  --vrnd VRND
  --value VALUE
