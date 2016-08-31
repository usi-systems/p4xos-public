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
	method Action handle1A(Bit#(16) bal);
	method Action handle1B(Bit#(16) bal, Bit#(16) vbal, Bit#(32) val, Bit#(16) acpt);
	method Action handle2A(Bit#(16) bal, Bit#(32) val);
	method Action handle2B(Bit#(16) bal, Bit#(32) val, Bit#(16) acpt);
endinterface

module mkPaxos#(PaxosIndication indication)(Paxos);
	Reg#(Bit#(16)) ballot <- mkReg(0);
	Reg#(Maybe#(Bit#(16))) vballot <- mkRegU;
	Reg#(Maybe#(Bit#(32))) value <- mkReg(tagged Invalid);

	Reg#(Bit#(16)) count1b <- mkReg(0);
	Reg#(Bit#(16)) count2b <- mkReg(0);
	Reg#(Bit#(16)) quorum <- mkReg(7);

	Reg#(Bit#(16)) hballot <- mkReg(0);
	Reg#(Maybe#(Bit#(32))) hval <- mkRegU;

	FIFO#(State) delay <- mkSizedFIFO(8);

	rule heard;
		delay.deq;
		let tmp = extend(pack(delay.first));
		indication.heard(tmp);
	endrule

	interface PaxosRequest request;

		method Action handle1A(Bit#(16) bal);
			State ret = IDLE;
			Maybe#(Bit#(16)) vbal = tagged Invalid;
			Maybe#(Bit#(32)) val = tagged Invalid;
			if (bal >= ballot) begin
				ballot <= bal;
				val = value;
				vbal = vballot;
			end
			delay.enq(ret);
		endmethod

		method Action handle1B(Bit#(16) bal, Bit#(16) vbal, Bit#(32) val, Bit#(16) acpt);
			State ret = IDLE;
			if (bal == ballot) begin
				if ((( 1 << acpt) & count1b) == 0) begin
					let cur_count = (1 << acpt) | count1b;
					count1b <= cur_count;
					if (vbal >= hballot) begin
						hballot <= vbal;
						hval <= tagged Valid val;
					end
					case (cur_count)
						'h03, 'h05, 'h06: ret = FINISH;
						default : ret = IDLE;
					endcase
				end
			end
			delay.enq(ret);
		endmethod

		method Action handle2A(Bit#(16) bal, Bit#(32) val);
			State ret = IDLE;
			if (bal >= ballot) begin
				ballot <= bal;
				vballot <= tagged Valid bal;
				value <= tagged Valid val;
				ret = ACCEPT;
			end
			delay.enq(ret);
		endmethod

		method Action handle2B(Bit#(16) bal, Bit#(32) val, Bit#(16) acpt);
			State ret = IDLE;
			if (bal == ballot) begin
				if ((( 1 << acpt) & count2b) == 0) begin
					let cur_count = (1 << acpt) | count2b;
					count2b <= cur_count;
					case (value) matches
						tagged Invalid:	value <= tagged Valid val;
						tagged Valid .v: if (v != val) ret = VALUE_ERROR;
					endcase
					case (cur_count)
						'h03, 'h05, 'h06: ret = FINISH;
						default : ret = IDLE;
					endcase
				end
			end
			else if (bal > ballot) begin
				ballot <= bal;
				value <= tagged Valid val;
				count2b <= (1 << acpt);
			end
			delay.enq(ret);
		endmethod
	endinterface

endmodule: mkPaxos
