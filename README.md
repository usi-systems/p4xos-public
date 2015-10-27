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

Run receiver on h1

```
./receive.py eth0
```

Run sender on h2

```
e.g,
./send.py --itf eth0 --dst 10.0.0.1

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
