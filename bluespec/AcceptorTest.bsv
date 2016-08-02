import Paxos::*;


(* synthesize *)
module mkAcceptorTest (Empty);
	Reg#(State) state <- mkReg(IDLE);
	Reg#(Bool) init <- mkReg(False);
	Reg#(Bool) recv1a <- mkReg(False);
	Reg#(Bool) recv2a <- mkReg(False);
	Reg#(Bit#(256)) value <- mkReg('1);
	PaxosIfc paxos <- mkPaxos(1, 2);

	rule stateIdle ( state == IDLE );
		if (!init) begin
			$display("Start phase 1");
			init <= True;
			recv2a <= True;
		end
		else if (recv1a) begin
			state <= PHASE1A;
		end
		else if (recv2a) begin
			state <= PHASE2A;
		end
		else
			state <= FINISH;
	endrule


	rule statePhase1A (state == PHASE1A);
		$display("Received Phase1A messages");
		int bal = 1;
		let res <- paxos.handle1A(bal);
		case (res) matches
			{ .s, tagged Valid .vb, tagged Valid .v } : begin $display("Accepted: %0d %x", vb, v); end
			{ .s, tagged Invalid, tagged Invalid } : begin $display("No value has been accepted"); end
		endcase
		state <= IDLE;
		recv1a <= False;
		recv2a <= False;
	endrule

	rule  statePhase2A (state == PHASE2A);
		$display("Received Phase2A messages");
		int bal = 1;
		let res <- paxos.handle2A(bal, value);
		state <= IDLE;
		recv1a <= True;
		recv2a <= False;
	endrule


	rule stateFinish (state == FINISH);
		$display("Test finished");
		$finish;
	endrule

	rule stateError (state == VALUE_ERROR);
		$display("Accepted values are different");
		$finish;
	endrule
endmodule
