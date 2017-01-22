BMV2_PATH=/home/bmv2
# P4C=/home/p4c/build/p4c-bm2-ss
P4C=/home/p4c-bm/p4c_bm/__main__.py
SWITCH_PATH=$(BMV2_PATH)/targets/simple_switch/simple_switch
CLI_PATH=$(BMV2_PATH)/tools/runtime_CLI.py
PROG=learner
all: coordinator acceptor learner

coordinator:
	# $(P4C) --p4-14 --toJSON coordinator.json coordinator.p4
	$(P4C) --json coordinator.json coordinator.p4

acceptor:
	$(P4C) --json acceptor.json acceptor.p4

learner:
	$(P4C) --json learner.json learner.p4

run:
	$(SWITCH_PATH) $(PROG).json \
    -i 0@veth0 -i 1@veth2 -i 2@veth4 -i 3@veth6 --log-console

config:
	$(CLI_PATH) --json $(PROG).json < commands.txt

kill:
	pkill lt-simple_sw

clean:
	rm *.json
