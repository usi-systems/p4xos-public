import Paxos::*;
import Vector::*;

(* synthesize *)
module mkMultiPaxosTest(Empty);
	Vector#(10,PaxosIfc) mpaxos <- replicateM(mkPaxos(1,2));
	Vector#(10, Reg#(State)) states <- replicateM(mkReg(IDLE));
	Vector#(10, Reg#(Bool)) inits <- replicateM(mkReg(False));
	Vector#(10, Reg#(Bool)) recv1as <- replicateM(mkReg(False));
	Vector#(10, Reg#(Bool)) recv2as <- replicateM(mkReg(False));
	Reg#(Bit#(256)) value <- mkReg('1);

	function Rules initInstance(int inst);
		return (rules
			rule initPaxos (states[inst] == IDLE);
				if (!inits[inst]) begin
					$display("Start phase 1 of instance %0d", inst);
					inits[inst] <= True;
					recv2as[inst] <= True;
				end
				else if (recv1as[inst]) begin
					states[inst] <= PHASE1A;
				end
				else if (recv2as[inst]) begin
					states[inst] <= PHASE2A;
				end
				else
					states[inst] <= FINISH;
			endrule
		endrules);
	endfunction

	function Rules state1A(int inst);
		return (rules
			rule  statePhase1A (states[inst] == PHASE1A);
				$display("Received Phase1A of instance %0d", inst);
				int bal = 1;
				let res <- mpaxos[inst].handle1A(bal);
				case (res) matches
					{ .s, tagged Valid .vb, tagged Valid .v } : begin $display("Accepted: %0d %x", vb, v); end
					{ .s, tagged Invalid, tagged Invalid } : begin $display("No value has been accepted"); end

				endcase
				states[inst] <= IDLE;
				recv1as[inst] <= False;
				recv2as[inst]  <= False;
			endrule
		endrules);
	endfunction

	function Rules state2A(int inst);
		return (rules
			rule  statePhase2A (states[inst] == PHASE2A);
				$display("Received Phase2A of instance %0d", inst);
				int bal = 1;
				let res <- mpaxos[inst].handle2A(bal, value);
				states[inst] <= IDLE;
				recv1as[inst] <= True;
				recv2as[inst] <= False;
			endrule
		endrules);
	endfunction

	function Rules paxosFinish(int inst);
		return ( rules
			rule stateFinish (states[inst] == FINISH);
				$display("Test finished");
				$finish;
			endrule
		endrules);
	endfunction


	for (int k = 0; k < 10; k = k + 1)
	begin
		addRules(initInstance(k));
		addRules(state1A(k));
		addRules(state2A(k));
		addRules(paxosFinish(k));
	end

endmodule
