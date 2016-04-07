`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 04/06/2016
// Module Name: p4_processor
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module p4_processor (

//#################################
//####       INTERFACES
//#################################

// AXIS PACKET INPUT INTERFACE
packet_in_packet_in_TVALID,
packet_in_packet_in_TREADY,
packet_in_packet_in_TDATA,
packet_in_packet_in_TKEEP,
packet_in_packet_in_TLAST,

// TUPLE INPUT INTERFACE
tuple_in_tuple_in_VALID,
tuple_in_tuple_in_DATA,

// AXI-LITE CONTROL INTERFACE
control_S_AXI_AWADDR,
control_S_AXI_AWVALID,
control_S_AXI_AWREADY,
control_S_AXI_WDATA,
control_S_AXI_WSTRB,
control_S_AXI_WVALID,
control_S_AXI_WREADY,
control_S_AXI_BRESP,
control_S_AXI_BVALID,
control_S_AXI_BREADY,
control_S_AXI_ARADDR,
control_S_AXI_ARVALID,
control_S_AXI_ARREADY,
control_S_AXI_RDATA,
control_S_AXI_RRESP,
control_S_AXI_RVALID,
control_S_AXI_RREADY,

// ENABLE SIGNAL
enable_processing,

// AXIS PACKET OUTPUT INTERFACE
packet_out_packet_out_TVALID,
packet_out_packet_out_TREADY,
packet_out_packet_out_TDATA,
packet_out_packet_out_TKEEP,
packet_out_packet_out_TLAST,

// TUPLE OUTPUT INTERFACE
tuple_out_tuple_out_VALID,
tuple_out_tuple_out_DATA,

// LINE CLK & RST SIGNALS
clk_line_rst,
clk_line,

// PACKET CLK & RST SIGNALS
clk_packet_rst,
clk_packet,

// CONTROL CLK & RST SIGNALS
clk_control_rst,
clk_control,

// RST DONE SIGNAL
internal_rst_done

);// synthesis black_box

//######################################
//####       TYPE OF INTERFACES
//######################################

// AXIS PACKET INPUT INTERFACE
input 		[0:0]										packet_in_packet_in_TVALID ;
output 		[0:0]										packet_in_packet_in_TREADY ;
input 		[255:0]										packet_in_packet_in_TDATA ;
input 		[31:0]										packet_in_packet_in_TKEEP ;
input 		[0:0]										packet_in_packet_in_TLAST ;

// TUPLE INPUT INTERFACE
input 		[0:0] 										tuple_in_tuple_in_VALID ;
input 		[127:0] 									tuple_in_tuple_in_DATA ;

// AXI-LITE CONTROL INTERFACE
input 		[8:0] 										control_S_AXI_AWADDR ;
input 		[0:0] 										control_S_AXI_AWVALID ;
output		[0:0] 										control_S_AXI_AWREADY ;
input 		[31:0] 										control_S_AXI_WDATA ;
input 		[3:0] 										control_S_AXI_WSTRB ;
input 		[0:0] 										control_S_AXI_WVALID ;
output 		[0:0] 										control_S_AXI_WREADY ;
output 		[1:0] 										control_S_AXI_BRESP ;
output 		[0:0] 										control_S_AXI_BVALID ;
input 		[0:0] 										control_S_AXI_BREADY ;
input 		[8:0] 										control_S_AXI_ARADDR ;
input 		[0:0] 										control_S_AXI_ARVALID ;
output 		[0:0] 										control_S_AXI_ARREADY ;
output 		[31:0]										control_S_AXI_RDATA ;
output 		[1:0] 										control_S_AXI_RRESP ;
output 		[0:0] 										control_S_AXI_RVALID ;
input 		[0:0] 										control_S_AXI_RREADY ;

// ENABLE SIGNAL
input 		[0:0] 										enable_processing ;

// AXIS PACKET OUTPUT INTERFACE
output 		[0:0] 										packet_out_packet_out_TVALID ;
input 		[0:0] 										packet_out_packet_out_TREADY ;
output 		[255:0]										packet_out_packet_out_TDATA ;
output 		[31:0]										packet_out_packet_out_TKEEP ;
output 		[0:0] 										packet_out_packet_out_TLAST ;

// TUPLE OUTPUT INTERFACE
output 		[0:0]										tuple_out_tuple_out_VALID ;
output 		[127:0]										tuple_out_tuple_out_DATA ;

// LINE CLK & RST SIGNALS
input 													clk_line_rst ;
input 													clk_line ;

// PACKET CLK & RST SIGNALS
input 													clk_packet_rst ;
input 													clk_packet ;

// CONTROL CLK & RST SIGNALS
input 													clk_control_rst ;
input 													clk_control ;

// RST DONE SIGNAL
output 		[0:0]										internal_rst_done ;

endmodule // p4_processor