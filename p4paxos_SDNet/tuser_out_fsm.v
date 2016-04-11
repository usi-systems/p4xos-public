`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 04/08/2016
// Module Name: tuser_out_fsm
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module tuser_out_fsm (

//#################################
//####       INTERFACES
//#################################

// CLK & RST
tout_aclk,
tout_arst,

// AXIS INPUT
tout_avalid,
tout_tlast,
//tout_adata,

// TUPLE INPUT
tout_valid,
tout_data,

// AXIS OUTPUT
tout_atuser

);// synthesis black_box

//######################################
//####       TYPE OF INTERFACES
//######################################


// CLK & RST
input 		[0:0]										tout_aclk ;
input 		[0:0]										tout_arst ;

// AXIS INPUT
input 		[0:0]										tout_avalid ;
input 		[0:0]										tout_tlast ;
//input 		[255:0]										tout_adata ;

// TUPLE INPUT
input 		[0:0]										tout_valid ;
input 		[127:0]										tout_data ;

// AXIS OUTPUT
output 		[127:0]										tout_atuser ;

//#################################
//####     WIRES & REGISTERS
//#################################

//#################################
//####   FINITE STATE MACHINE
//#################################

endmodule // tuser_out_fsm