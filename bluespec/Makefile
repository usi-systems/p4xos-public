BSV_FILE=Tb.bsv
INTERFACE=Tb

all: compile convert run

compile:
	bsc -u -verilog ${BSV_FILE}
convert:
	bsc -o sim -e mk${INTERFACE} mk${INTERFACE}.v
run:
	./sim

test:
	bsc -u -verilog AcceptorTest.bsv
	bsc -o test -e mkAcceptorTest mkAcceptorTest.v
	./test

multi:
	bsc -u -verilog MultiPaxosTest.bsv
	bsc -o multi -e mkMultiPaxosTest mkMultiPaxosTest.v
	./multi
clean:
	rm -rf test sim multi *.v *.bo
