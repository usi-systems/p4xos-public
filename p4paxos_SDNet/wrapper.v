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
wire		[0:0]										reg_packet_in_packet_in_TVALID ;
wire 		[0:0]										reg_packet_in_packet_in_TREADY ;
wire		[255:0]										reg_packet_in_packet_in_TDATA ;
wire		[31:0]										reg_packet_in_packet_in_TKEEP ;
wire		[0:0]										reg_packet_in_packet_in_TLAST ;

// AXI-LITE CONTROL INTERFACE
wire		[8:0] 										reg_control_S_AXI_AWADDR ;
wire		[0:0] 										reg_control_S_AXI_AWVALID ;
wire		[0:0] 										reg_control_S_AXI_AWREADY ;
wire		[31:0] 										reg_control_S_AXI_WDATA ;
wire 		[3:0] 										reg_control_S_AXI_WSTRB ;
wire 		[0:0] 										reg_control_S_AXI_WVALID ;
wire 		[0:0] 										reg_control_S_AXI_WREADY ;
wire 		[1:0] 										reg_control_S_AXI_BRESP ;
wire 		[0:0] 										reg_control_S_AXI_BVALID ;
wire 		[0:0] 										reg_control_S_AXI_BREADY ;
wire 		[8:0] 										reg_control_S_AXI_ARADDR ;
wire 		[0:0] 										reg_control_S_AXI_ARVALID ;
wire 		[0:0] 										reg_control_S_AXI_ARREADY ;
wire 		[31:0]										reg_control_S_AXI_RDATA ;
wire 		[1:0] 										reg_control_S_AXI_RRESP ;
wire 		[0:0] 										reg_control_S_AXI_RVALID ;
wire 		[0:0] 										reg_control_S_AXI_RREADY ;

// AXIS PACKET OUTPUT INTERFACE
wire 		[0:0] 										reg_packet_out_packet_out_TVALID ;
wire 		[0:0] 										reg_packet_out_packet_out_TREADY ;
wire 		[255:0]										reg_packet_out_packet_out_TDATA ;
wire 		[31:0]										reg_packet_out_packet_out_TKEEP ;
wire 		[0:0] 										reg_packet_out_packet_out_TLAST ;

// LINE CLK & RST SIGNALS
reg 													reg_clk_line_rst ; // INV
wire 													reg_clk_line ;

// PACKET CLK & RST SIGNALS
reg 													reg_clk_packet_rst ; // INV
wire 													reg_clk_packet ;

// CONTROL CLK & RST SIGNALS
reg 													reg_clk_control_rst ; // INV
wire 													reg_clk_control ;

//#################################
//####       CONNECTIONS
//#################################

// AXIS PACKET INPUT INTERFACE
assign 		reg_packet_in_packet_in_TVALID = s_axis_tvalid ;
assign 		reg_packet_in_packet_in_TREADY = s_axis_tready;
assign 		reg_packet_in_packet_in_TDATA = s_axis_tdata;
assign 		reg_packet_in_packet_in_TKEEP = s_axis_tkeep;
assign 		reg_packet_in_packet_in_TLAST = s_axis_tlast;

// AXI-LITE CONTROL INTERFACE
assign		reg_control_S_AXI_AWADDR = S_AXI_AWADDR; // [REG]<--INPUT
assign		reg_control_S_AXI_AWVALID = S_AXI_AWVALID ; // [REG]<--INPUT
assign		S_AXI_AWREADY = reg_control_S_AXI_AWREADY; // OUTPUT<--[REG]
assign		reg_control_S_AXI_WDATA = S_AXI_WDATA ; // [REG]<--INPUT
assign		reg_control_S_AXI_WSTRB = S_AXI_WSTRB ; // [REG]<--INPUT
assign		reg_control_S_AXI_WVALID = S_AXI_WVALID ; // [REG]<--INPUT
assign		S_AXI_WREADY = reg_control_S_AXI_WREADY ; // OUTPUT<--[REG]
assign		S_AXI_BRESP = reg_control_S_AXI_BRESP ; // OUTPUT<--[REG]
assign		S_AXI_BVALID = reg_control_S_AXI_BVALID ; // OUTPUT<--[REG]
assign		reg_control_S_AXI_BREADY = S_AXI_BREADY; // [REG]<--INPUT
assign		reg_control_S_AXI_ARADDR = S_AXI_ARADDR; // [REG]<--INPUT
assign		reg_control_S_AXI_ARVALID = S_AXI_ARVALID ; // [REG]<--INPUT
assign		S_AXI_ARREADY = reg_control_S_AXI_ARREADY ; // OUTPUT<--[REG]
assign		S_AXI_RDATA = reg_control_S_AXI_RDATA ; // OUTPUT<--[REG]
assign		S_AXI_RRESP = reg_control_S_AXI_RRESP ; // OUTPUT<--[REG]
assign		S_AXI_RVALID = reg_control_S_AXI_RVALID ; // OUTPUT<--[REG]
assign		reg_control_S_AXI_RREADY = S_AXI_RREADY; // [REG]<--INPUT

// AXIS PACKET OUTPUT INTERFACE
assign  	m_axis_tvalid	=   reg_packet_out_packet_out_TVALID ;
assign  	m_axis_tready	=	reg_packet_out_packet_out_TREADY ;
assign  	m_axis_tdata	=	reg_packet_out_packet_out_TDATA ;
assign  	m_axis_tkeep	=	reg_packet_out_packet_out_TKEEP ;
assign  	m_axis_tlast	=	reg_packet_out_packet_out_TLAST ;

// LINE CLK & RST SIGNALS
assign 		reg_clk_line = axis_aclk ;

// PACKET CLK & RST SIGNALS
assign 		reg_clk_packet = axis_aclk ;

// CONTROL CLK & RST SIGNALS
assign  	reg_clk_control = S_AXI_ACLK ;

//###################################
//####       P4_PROCESSOR INSTANCE
//###################################

p4_processor p4_processor_inst (

// AXIS PACKET INPUT INTERFACE
.packet_in_packet_in_TVALID								(reg_packet_in_packet_in_TVALID),
.packet_in_packet_in_TREADY								(reg_packet_in_packet_in_TREADY),
.packet_in_packet_in_TDATA								(reg_packet_in_packet_in_TDATA),
.packet_in_packet_in_TKEEP								(reg_packet_in_packet_in_TKEEP),
.packet_in_packet_in_TLAST								(reg_packet_in_packet_in_TLAST),

// TUPLE INPUT INTERFACE
.tuple_in_tuple_in_VALID								(1'b0), // CONSTANT
.tuple_in_tuple_in_DATA									(128'b0), // CONSTANT

// AXI-LITE CONTROL INTERFACE
.control_S_AXI_AWADDR									(reg_control_S_AXI_AWADDR [8:0]  ), // MISMATCH [11 : 9]
.control_S_AXI_AWVALID									(reg_control_S_AXI_AWVALID ),
.control_S_AXI_AWREADY									(reg_control_S_AXI_AWREADY ),
.control_S_AXI_WDATA									(reg_control_S_AXI_WDATA ),
.control_S_AXI_WSTRB									(reg_control_S_AXI_WSTRB  ),
.control_S_AXI_WVALID									(reg_control_S_AXI_WVALID  ),
.control_S_AXI_WREADY									(reg_control_S_AXI_WREADY  ),
.control_S_AXI_BRESP									(reg_control_S_AXI_BRESP  ),
.control_S_AXI_BVALID									(reg_control_S_AXI_BVALID  ),
.control_S_AXI_BREADY									(reg_control_S_AXI_BREADY  ),
.control_S_AXI_ARADDR									(reg_control_S_AXI_ARADDR [8:0] ), // MISMATCH [11 : 9]
.control_S_AXI_ARVALID									(reg_control_S_AXI_ARVALID ),
.control_S_AXI_ARREADY									(reg_control_S_AXI_ARREADY ),
.control_S_AXI_RDATA									(reg_control_S_AXI_RDATA  ),
.control_S_AXI_RRESP									(reg_control_S_AXI_RRESP  ),
.control_S_AXI_RVALID									(reg_control_S_AXI_RVALID  ),
.control_S_AXI_RREADY									(reg_control_S_AXI_RREADY  ),

// ENABLE SIGNAL
.enable_processing										(1'b1), // CONSTANT

// AXIS PACKET OUTPUT INTERFACE
.packet_out_packet_out_TVALID							(reg_packet_out_packet_out_TVALID  ),
.packet_out_packet_out_TREADY							(reg_packet_out_packet_out_TREADY  ),
.packet_out_packet_out_TDATA							(reg_packet_out_packet_out_TDATA   ),
.packet_out_packet_out_TKEEP							(reg_packet_out_packet_out_TKEEP   ),
.packet_out_packet_out_TLAST							(reg_packet_out_packet_out_TLAST   ),

// TUPLE OUTPUT INTERFACE
.tuple_out_tuple_out_VALID								(/*VOID*/), // N.C.
.tuple_out_tuple_out_DATA								(/*VOID*/), // N.C.

// LINE CLK & RST SIGNALS
.clk_line_rst											(reg_clk_line_rst ), // INV
.clk_line												(reg_clk_line ),

// PACKET CLK & RST SIGNALS
.clk_packet_rst											(reg_clk_packet_rst), // INV
.clk_packet												(reg_clk_packet),

// CONTROL CLK & RST SIGNALS
.clk_control_rst										(reg_clk_control_rst), // INV
.clk_control											(reg_clk_control),

// RST DONE SIGNAL
.internal_rst_done										(/*VOID*/) // N.C.
  
); // p4_processor_inst

//#################################
//####       ALWAYS
//#################################

always @ ( posedge axis_aclk /*or posedge S_AXI_ACLK*/ )
begin

// TUSER
m_axis_tuser <= s_axis_tuser;

// INVERT RESET REGISTERS
reg_clk_line_rst <= ~(axis_resetn) ; // INV
reg_clk_packet_rst <= ~(axis_resetn) ; // INV
reg_clk_control_rst <= ~(S_AXI_ARESETN) ; // INV

end // always

endmodule // wrapper