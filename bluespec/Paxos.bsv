typedef enum { IDLE, PHASE1A, PHASE1B, PHASE2A, PHASE2B, ACCEPT, VALUE_ERROR, FINISH } State deriving (Bits, Eq);

interface PaxosIfc;
	method ActionValue#(Tuple3#(State, Maybe#(int), Maybe#(Bit#(256)))) handle1A(int bal);
	method ActionValue#(State) handle1B(int bal);
	method ActionValue#(State) handle2A(int bal, Bit#(256) val);
	method ActionValue#(State) handle2B(int bal, Bit#(256) val);
endinterface: PaxosIfc

(* synthesize *)
module mkPaxos#(parameter int init_ballot, parameter int qsize)(PaxosIfc);
	Reg#(int) ballot <- mkReg(init_ballot);
	Reg#(int) quorum <- mkReg(qsize);
	Reg#(int) vballot <- mkRegU;
	Reg#(Bit#(256)) value <- mkRegU;
	Reg#(int) count1b <- mkReg(0);
	Reg#(int) count2b <- mkReg(0);
	Reg#(int) uninitialized <- mkRegU;


	method ActionValue#(Tuple3#(State, Maybe#(int), Maybe#(Bit#(256)))) handle1A(int bal);
		State ret = IDLE;
		Maybe#(int) vbal = tagged Invalid;
		Maybe#(Bit#(256)) val = tagged Invalid;
		if (bal >= ballot) begin
			ballot <= bal;
			if (vballot != uninitialized) begin
				vbal = tagged Valid vballot;
				val = tagged Valid value;
			end
			else begin
				vbal = tagged Invalid;
				val = tagged Invalid;
			end
		end
		return tuple3(IDLE, vbal, val);
	endmethod

	method ActionValue#(State) handle1B(int bal);
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
		return ret;
	endmethod

	method ActionValue#(State) handle2A(int bal, Bit#(256) val);
		State ret = IDLE;
		if (bal >= ballot) begin
			ballot <= bal;
			vballot <= bal;
			value <= val;
			ret = ACCEPT;
		end
		return ret;
	endmethod

	method ActionValue#(State) handle2B(int bal, Bit#(256) val);
		State ret = IDLE;
		if (bal >= ballot) begin
			if ((value != 'haaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)
				&& (value != val)) begin
				ret = VALUE_ERROR;
			end
			else begin
				ballot <= bal;
				vballot <= bal;
				value <= val;
				if (count2b == quorum - 1) begin
					count2b <= count2b + 1;
					ret = FINISH;
				end
				else begin
					count2b <= count2b + 1;
				end
			end
		end
		return ret;
	endmethod
endmodule: mkPaxos
