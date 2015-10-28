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

You can either run UDP server or packet sniffer on h1

## UDP socket server

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

## Scapy - packet generator

Run sender on h2

```
./route-add.sh
./send.py --itf eth0 --dst 224.3.29.71

usage: send.py [-h] [--dst DST] [--itf ITF] [--typ TYP] [--rnd RND]
               [--vrnd VRND] [--value VALUE]

Generate Paxos messages.

optional arguments:
  -h, --help     show this help message and exit
  --dst DST
  --itf ITF
  --typ TYP
  --rnd RND
  --vrnd VRND
  --value VALUE
```
