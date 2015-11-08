# Build behavioral-model
Required: [p4-lang/behavioral-model](https://github.com/p4lang/behavioral-model) and [p4-lang/p4c-bm](https://github.com/p4lang/p4c-bm)


# Demo

Change to the **bmv2** directory,
```
cd bmv2
```

In another terminal, run demo

```
sudo ./run_demo.sh
```


From mininet prompt

```
mininet> xterm h1 h2
```

## Scapy - packet sniffer

Run packet sniffer on h2 (or h3)

```
cd scripts/
./receive.py
```

## UDP Client

Run client on h1 (or h4)

```
cd scripts
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
