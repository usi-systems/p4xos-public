import FIFO::*;
import Vector::*;


interface PaxosIndication;
	method Action heard(Bit#(32) v);
endinterface

interface Paxos;
	interface PaxosRequest request;
endinterface

typedef enum { IDLE, PHASE1A, PHASE1B, PHASE2A, PHASE2B, ACCEPT, VALUE_ERROR, FINISH } State deriving (Bits, Eq);

interface PaxosRequest;
	method Action handle1A(Bit#(32) bal);
	method Action handle1B(Bit#(32) bal, Bit#(32) vbal, Bit#(32) val);
	method Action handle2A(Bit#(32) bal, Bit#(32) val);
	method Action handle2B(Bit#(32) bal, Bit#(32) val);
endinterface

module mkPaxos#(PaxosIndication indication)(Paxos);
	Reg#(Bit#(32)) ballot <- mkReg(0);
	Reg#(Bit#(32)) quorum <- mkReg(2);
	Reg#(Bit#(32)) vballot <- mkRegU;
	Reg#(Maybe#(Bit#(32))) value <- mkReg(tagged Invalid);
	Reg#(Bit#(32)) count1b <- mkReg(0);
	Reg#(Bit#(32)) count2b <- mkReg(0);
	Reg#(Bit#(32)) uninitialized <- mkRegU;

	FIFO#(State) delay <- mkSizedFIFO(8);

	rule heard;
		delay.deq;
		let tmp = extend(pack(delay.first));
		indication.heard(tmp);
	endrule

	interface PaxosRequest request;

		method Action handle1A(Bit#(32) bal);
			State ret = IDLE;
			Maybe#(Bit#(32)) vbal = tagged Invalid;
			Maybe#(Bit#(32)) val = tagged Invalid;
			if (bal >= ballot) begin
				ballot <= bal;
				if (vballot != uninitialized) begin
					vbal = tagged Valid vballot;
					val = value;
				end
				else begin
					vbal = tagged Invalid;
				end
			end
			delay.enq(ret);
		endmethod

		method Action handle1B(Bit#(32) bal, Bit#(32) vbal, Bit#(32) val);
			State ret = IDLE;
			if (bal >= ballot) begin
				ballot <= bal;
				if (count1b == quorum - 1) begin
					count1b <= count1b + 1;
					ret = PHASE2A;
				end
				else begin
					count1b <= count1b + 1;
				end
			end
			delay.enq(ret);
		endmethod

		method Action handle2A(Bit#(32) bal, Bit#(32) val);
			State ret = IDLE;
			if (bal >= ballot) begin
				ballot <= bal;
				vballot <= bal;
				value <= tagged Valid val;
				ret = ACCEPT;
			end
			delay.enq(ret);
		endmethod

		method Action handle2B(Bit#(32) bal, Bit#(32) val);
			State ret = IDLE;
			if (bal == ballot) begin
				case(value) matches
					tagged Invalid:	value <= tagged Valid val;
					tagged Valid .v: if (v != val) ret = VALUE_ERROR;
				endcase
				if (count2b == quorum - 1) begin
					count2b <= count2b + 1;
					ret = FINISH;
				end
				else begin
					count2b <= count2b + 1;
				end
			end
			else if (bal > ballot) begin
				ballot <= bal;
				value <= tagged Valid val;
				count2b <= 1;
			end
			delay.enq(ret);
		endmethod
	endinterface

endmodule: mkPaxos
