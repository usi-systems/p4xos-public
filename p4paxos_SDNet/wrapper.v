`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 04/06/2016
// Module Name: wrapper
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module wrapper#(

//#################################
//####       PARAMETERS
//#################################

//Master AXI Stream Data Width
parameter 												C_M_AXIS_DATA_WIDTH = 256,
parameter 												C_S_AXIS_DATA_WIDTH = 256,
parameter 												C_M_AXIS_TUSER_WIDTH = 128,
parameter 												C_S_AXIS_TUSER_WIDTH = 128,

// AXI Registers Data Width
parameter 												C_S_AXI_DATA_WIDTH = 32,
parameter 												C_S_AXI_ADDR_WIDTH = 12

)
(

//#################################
//####       INTERFACES
//#################################

// AXIS CLK & RST SIGNALS
input 													axis_aclk,
input 													axis_resetn,

// AXIS PACKET OUTPUT INTERFACE
output 		[C_M_AXIS_DATA_WIDTH - 1:0] 				m_axis_tdata,
output 		[((C_M_AXIS_DATA_WIDTH / 8)) - 1:0] 		m_axis_tkeep,
output reg	[C_M_AXIS_TUSER_WIDTH-1:0] 					m_axis_tuser,
output 													m_axis_tvalid,
input 													m_axis_tready,
output 													m_axis_tlast,

// AXIS PACKET INPUT INTERFACE
input 		[C_S_AXIS_DATA_WIDTH - 1:0] 				s_axis_tdata,
input 		[((C_S_AXIS_DATA_WIDTH / 8)) - 1:0] 		s_axis_tkeep,
input 		[C_S_AXIS_TUSER_WIDTH-1:0] 					s_axis_tuser,
input 													s_axis_tvalid,
output 													s_axis_tready,
input 													s_axis_tlast,

// AXI CLK & RST SIGNALS
input 													S_AXI_ACLK,
input 													S_AXI_ARESETN,

// AXI-LITE CONTROL INTERFACE
input		[C_S_AXI_ADDR_WIDTH-1 : 0]					S_AXI_AWADDR,
input 													S_AXI_AWVALID,
input 		[C_S_AXI_DATA_WIDTH-1 : 0]					S_AXI_WDATA,
input 		[C_S_AXI_DATA_WIDTH/8-1 : 0]				S_AXI_WSTRB,
input 													S_AXI_WVALID,
input 													S_AXI_BREADY,
input 		[C_S_AXI_ADDR_WIDTH-1 : 0]					S_AXI_ARADDR,
input 													S_AXI_ARVALID,
input 													S_AXI_RREADY,
output													S_AXI_ARREADY,
output		[C_S_AXI_DATA_WIDTH-1 : 0]					S_AXI_RDATA,
output		[1 : 0]										S_AXI_RRESP,
output													S_AXI_RVALID,
output													S_AXI_WREADY,
output 		[1 :0]										S_AXI_BRESP,
output													S_AXI_BVALID,
output													S_AXI_AWREADY

);

//#################################
//####     WIRES & REGISTERS
//#################################

// AXIS PACKET INPUT INTERFACE
wire		[0:0]										wire_packet_in_packet_in_TVALID ;
wire 		[0:0]										wire_packet_in_packet_in_TREADY ;
wire		[255:0]										wire_packet_in_packet_in_TDATA ;
wire		[127:0]										wire_packet_in_packet_in_TUSER ;
wire		[31:0]										wire_packet_in_packet_in_TKEEP ;
wire		[0:0]										wire_packet_in_packet_in_TLAST ;

// AXI-LITE CONTROL INTERFACE
wire		[8:0] 										wire_control_S_AXI_AWADDR ;
wire		[0:0] 										wire_control_S_AXI_AWVALID ;
wire		[0:0] 										wire_control_S_AXI_AWREADY ;
wire		[31:0] 										wire_control_S_AXI_WDATA ;
wire 		[3:0] 										wire_control_S_AXI_WSTRB ;
wire 		[0:0] 										wire_control_S_AXI_WVALID ;
wire 		[0:0] 										wire_control_S_AXI_WREADY ;
wire 		[1:0] 										wire_control_S_AXI_BRESP ;
wire 		[0:0] 										wire_control_S_AXI_BVALID ;
wire 		[0:0] 										wire_control_S_AXI_BREADY ;
wire 		[8:0] 										wire_control_S_AXI_ARADDR ;
wire 		[0:0] 										wire_control_S_AXI_ARVALID ;
wire 		[0:0] 										wire_control_S_AXI_ARREADY ;
wire 		[31:0]										wire_control_S_AXI_RDATA ;
wire 		[1:0] 										wire_control_S_AXI_RRESP ;
wire 		[0:0] 										wire_control_S_AXI_RVALID ;
wire 		[0:0] 										wire_control_S_AXI_RREADY ;

// AXIS PACKET OUTPUT INTERFACE
wire 		[0:0] 										wire_packet_out_packet_out_TVALID ;
wire 		[0:0] 										wire_packet_out_packet_out_TREADY ;
wire 		[255:0]										wire_packet_out_packet_out_TDATA ;
wire 		[127:0]										wire_packet_out_packet_out_TUSER ;
wire 		[31:0]										wire_packet_out_packet_out_TKEEP ;
wire 		[0:0] 										wire_packet_out_packet_out_TLAST ;

// TUPLE INPUT INTERFACE
wire 		[0:0] 										wire_tin_valid ;
wire 		[127:0]										wire_tin_data ;

// TUPLE OUTPUT INTERFACE
wire 		[0:0] 										wire_tout_valid ;
wire 		[127:0]										wire_tout_data ;

// LINE CLK & RST SIGNALS
reg 													reg_clk_line_rst ; // INV
wire 													wire_clk_line ;

// PACKET CLK & RST SIGNALS
reg 													reg_clk_packet_rst ; // INV
wire 													wire_clk_packet ;

// CONTROL CLK & RST SIGNALS
reg 													reg_clk_control_rst ; // INV
wire 													wire_clk_control ;

// TUPLE CLK & RST SIGNALS
reg                                                     reg_tuple_rst ;
wire                                                    wire_clk_tuple ;

//#################################
//####       CONNECTIONS
//#################################

// AXIS PACKET INPUT INTERFACE
assign 		wire_packet_in_packet_in_TVALID = s_axis_tvalid ;
assign 		wire_packet_in_packet_in_TREADY = s_axis_tready;
assign 		wire_packet_in_packet_in_TDATA = s_axis_tdata;
assign 		wire_packet_in_packet_in_TUSER = s_axis_tuser;
assign 		wire_packet_in_packet_in_TKEEP = s_axis_tkeep;
assign 		wire_packet_in_packet_in_TLAST = s_axis_tlast;


// AXI-LITE CONTROL INTERFACE
assign		wire_control_S_AXI_AWADDR = S_AXI_AWADDR; // [REG]<--INPUT
assign		wire_control_S_AXI_AWVALID = S_AXI_AWVALID ; // [REG]<--INPUT
assign		S_AXI_AWREADY = wire_control_S_AXI_AWREADY; // OUTPUT<--[REG]
assign		wire_control_S_AXI_WDATA = S_AXI_WDATA ; // [REG]<--INPUT
assign		wire_control_S_AXI_WSTRB = S_AXI_WSTRB ; // [REG]<--INPUT
assign		wire_control_S_AXI_WVALID = S_AXI_WVALID ; // [REG]<--INPUT
assign		S_AXI_WREADY = wire_control_S_AXI_WREADY ; // OUTPUT<--[REG]
assign		S_AXI_BRESP = wire_control_S_AXI_BRESP ; // OUTPUT<--[REG]
assign		S_AXI_BVALID = wire_control_S_AXI_BVALID ; // OUTPUT<--[REG]
assign		wire_control_S_AXI_BREADY = S_AXI_BREADY; // [REG]<--INPUT
assign		wire_control_S_AXI_ARADDR = S_AXI_ARADDR; // [REG]<--INPUT
assign		wire_control_S_AXI_ARVALID = S_AXI_ARVALID ; // [REG]<--INPUT
assign		S_AXI_ARREADY = wire_control_S_AXI_ARREADY ; // OUTPUT<--[REG]
assign		S_AXI_RDATA = wire_control_S_AXI_RDATA ; // OUTPUT<--[REG]
assign		S_AXI_RRESP = wire_control_S_AXI_RRESP ; // OUTPUT<--[REG]
assign		S_AXI_RVALID = wire_control_S_AXI_RVALID ; // OUTPUT<--[REG]
assign		wire_control_S_AXI_RREADY = S_AXI_RREADY; // [REG]<--INPUT

// AXIS PACKET OUTPUT INTERFACE
assign  	m_axis_tvalid	=   wire_packet_out_packet_out_TVALID ;
assign  	m_axis_tready	=	wire_packet_out_packet_out_TREADY ;
assign  	m_axis_tdata	=	wire_packet_out_packet_out_TDATA ;
assign  	m_axis_tkeep	=	wire_packet_out_packet_out_TKEEP ;
assign  	m_axis_tlast	=	wire_packet_out_packet_out_TLAST ;

// LINE RST SIGNAL
assign 		wire_clk_line = axis_aclk ;

// PACKET RST SIGNAL
assign 		wire_clk_packet = axis_aclk ;

// CONTROL RST SIGNAL
assign  	wire_clk_control = S_AXI_ACLK ;

// TUPLE CLK SIGNAL
assign 		wire_clk_tuple = axis_aclk ;

//###################################
//####    AXI_CONTROL INSTANCE
//###################################

// TODO:
// HW MODULE THAT MAPS SUME AXI CONTROL
// ADDRESS TO P4_PROCESSOR AXI CONTROL ADDRESS
// INSTEAD OF DISCARDING EXCEEDING bits

//###################################
//####    TUSER_IN_FSM INSTANCE
//###################################

tuser_in_fsm tuser_in_fsm_inst (

// CLK & RST
.tin_aclk												(wire_clk_tuple),
.tin_arst												(reg_tuple_rst),

// AXIS INPUT
.tin_avalid												(wire_packet_in_packet_in_TVALID),
//.tin_adata												(wire_packet_in_packet_in_TDATA),
.tin_atuser												(wire_packet_in_packet_in_TUSER),
.tin_atlast												(wire_packet_in_packet_in_TLAST),

// TUPLE OUTPUT
.tin_valid												(wire_tin_valid),
.tin_data												(wire_tin_data)

); // tuser_in_fsm_inst

//###################################
//####    TUSER_OUT_FSM INSTANCE
//###################################

tuser_out_fsm tuser_out_fsm_inst (

// CLK & RST
.tout_aclk												(wire_clk_tuple),
.tout_arst												(reg_tuple_rst),

// AXIS INPUT
.tout_avalid											(wire_packet_out_packet_out_TVALID),
.tout_tlast												(wire_packet_out_packet_out_TLAST),
//.tout_adata												(wire_packet_out_packet_out_TDATA),

// TUPLE INPUT
.tout_valid												(wire_tout_valid),
.tout_data												(wire_tout_data),


// TUSER OUTPUT
.tout_atuser											(wire_packet_out_packet_out_TUSER)

); // tuser_out_fsm_inst

//###################################
//####    P4_PROCESSOR INSTANCE
//###################################

p4_processor p4_processor_inst (

// AXIS PACKET INPUT INTERFACE
.packet_in_packet_in_TVALID								(wire_packet_in_packet_in_TVALID),
.packet_in_packet_in_TREADY								(wire_packet_in_packet_in_TREADY),
.packet_in_packet_in_TDATA								(wire_packet_in_packet_in_TDATA),
.packet_in_packet_in_TKEEP								(wire_packet_in_packet_in_TKEEP),
.packet_in_packet_in_TLAST								(wire_packet_in_packet_in_TLAST),

// TUPLE INPUT INTERFACE
.tuple_in_tuple_in_VALID								(wire_tin_valid), // TIN FSM
.tuple_in_tuple_in_DATA									(wire_tin_data), // TIN FSM

// AXI-LITE CONTROL INTERFACE
.control_S_AXI_AWADDR									(wire_control_S_AXI_AWADDR [8:0]  ), // MISMATCH [11 : 9]
.control_S_AXI_AWVALID									(wire_control_S_AXI_AWVALID ),
.control_S_AXI_AWREADY									(wire_control_S_AXI_AWREADY ),
.control_S_AXI_WDATA									(wire_control_S_AXI_WDATA ),
.control_S_AXI_WSTRB									(wire_control_S_AXI_WSTRB  ),
.control_S_AXI_WVALID									(wire_control_S_AXI_WVALID  ),
.control_S_AXI_WREADY									(wire_control_S_AXI_WREADY  ),
.control_S_AXI_BRESP									(wire_control_S_AXI_BRESP  ),
.control_S_AXI_BVALID									(wire_control_S_AXI_BVALID  ),
.control_S_AXI_BREADY									(wire_control_S_AXI_BREADY  ),
.control_S_AXI_ARADDR									(wire_control_S_AXI_ARADDR [8:0] ), // MISMATCH [11 : 9]
.control_S_AXI_ARVALID									(wire_control_S_AXI_ARVALID ),
.control_S_AXI_ARREADY									(wire_control_S_AXI_ARREADY ),
.control_S_AXI_RDATA									(wire_control_S_AXI_RDATA  ),
.control_S_AXI_RRESP									(wire_control_S_AXI_RRESP  ),
.control_S_AXI_RVALID									(wire_control_S_AXI_RVALID  ),
.control_S_AXI_RREADY									(wire_control_S_AXI_RREADY  ),

// ENABLE SIGNAL
.enable_processing										(1'b1), // CONSTANT

// AXIS PACKET OUTPUT INTERFACE
.packet_out_packet_out_TVALID							(wire_packet_out_packet_out_TVALID  ),
.packet_out_packet_out_TREADY							(wire_packet_out_packet_out_TREADY  ),
.packet_out_packet_out_TDATA							(wire_packet_out_packet_out_TDATA   ),
.packet_out_packet_out_TKEEP							(wire_packet_out_packet_out_TKEEP   ),
.packet_out_packet_out_TLAST							(wire_packet_out_packet_out_TLAST   ),

// TUPLE OUTPUT INTERFACE
.tuple_out_tuple_out_VALID								(wire_tout_valid), // TOUT FSM
.tuple_out_tuple_out_DATA								(wire_tout_data), // TOUT FSM

// LINE CLK & RST SIGNALS
.clk_line_rst											(reg_clk_line_rst ), // INV
.clk_line												(wire_clk_line ),

// PACKET CLK & RST SIGNALS
.clk_packet_rst											(reg_clk_packet_rst), // INV
.clk_packet												(wire_clk_packet),

// CONTROL CLK & RST SIGNALS
.clk_control_rst										(reg_clk_control_rst), // INV
.clk_control											(wire_clk_control),

// RST DONE SIGNAL
.internal_rst_done										(/*VOID*/) // N.C.
  
); // p4_processor_inst

//#################################
//####       ALWAYS
//#################################

always @ ( posedge axis_aclk /*or posedge S_AXI_ACLK*/ )

begin

// INVERT RESET REGISTERS
reg_clk_line_rst <= ~(axis_resetn) ; // INV
reg_clk_packet_rst <= ~(axis_resetn) ; // INV
reg_clk_control_rst <= ~(S_AXI_ARESETN) ; // INV
reg_tuple_rst <= ~(axis_resetn) ; // INV

// TUSER OUTPUT REGISTER
m_axis_tuser <=	wire_packet_out_packet_out_TUSER ;

end // always

endmodule // wrapper