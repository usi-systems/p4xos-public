package Tb;

import Paxos::*;


(* synthesize *)
module mkTb (Empty);
	Reg#(State) state <- mkReg(IDLE);
	Reg#(Bool) init <- mkReg(False);
	Reg#(Bool) recv1b <- mkReg(False);
	Reg#(Bool) recv2b <- mkReg(False);
	Reg#(Bit#(256)) value <- mkReg('1);
	PaxosIfc paxos <- mkPaxos(1, 2);

	rule stateIdle ( state == IDLE );
		if (!init) begin
			$display("Start phase 1");
			init <= True;
			state <= PHASE1A;
		end
		else if (recv1b) begin
			state <= PHASE1B;
		end
		else if (recv2b) begin
			state <= PHASE2B;
		end
	endrule

	rule statePhase1A (state == PHASE1A);
		state <= IDLE;
		recv1b <= True;
		recv2b <= False;
	endrule

	rule statePhase1B (state == PHASE1B);
		$display("Received Phase1B messages");
		let ret <- paxos.handle1B(1);
		state <= ret;
	endrule

	rule  statePhase2A (state == PHASE2A);
		$display("Sent Phase2A messages");
		state <= IDLE;
		recv1b <= False;
		recv2b <= True;
	endrule

	rule statePhase2B (state == PHASE2B);
		$display("Received Phase2B messages");
		let ret <- paxos.handle2B(1, value);
		if (ret != IDLE)
			recv2b <= False;
		state <= ret;
	endrule

	rule stateFinish (state == FINISH);
		$display("Paxos has finished");
		$finish;
	endrule

	rule stateError (state == VALUE_ERROR);
		$display("Accepted values are different");
		$finish;
	endrule
endmodule: mkTb


endpackage: Tb
