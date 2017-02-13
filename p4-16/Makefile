BMV2_PATH=/home/bmv2
P4C=/home/p4c/build/p4c-bm2-ss
# P4C=/home/p4c-bm/p4c_bm/__main__.py
SWITCH_PATH=$(BMV2_PATH)/targets/simple_switch/simple_switch
CLI_PATH=$(BMV2_PATH)/tools/runtime_CLI.py


run-%: %.json
	$(SWITCH_PATH) $^ \
    -i 0@veth0 -i 1@veth2 -i 2@veth4 -i 3@veth6 --log-console

config-%: %.json
	$(CLI_PATH) --json $^ < $@.txt

%.json : p4src/%.p4
	$(P4C) --p4-16 -o $@ $^

kill:
	pkill lt-simple_sw

clean:
	rm *.json