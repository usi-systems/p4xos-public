`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 04/08/2016
// Module Name: tuser_in_fsm
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module tuser_in_fsm (

//#################################
//####       INTERFACES
//#################################

// CLK & RST
tin_aclk,
tin_arst,

// AXIS INPUT
tin_avalid,
tin_adata,
tin_atuser,

// TUPLE OUTPUT
tin_valid,
tin_data

);// synthesis black_box

//######################################
//####       TYPE OF INTERFACES
//######################################


// CLK & RST
input 		[0:0]										tin_aclk ;
input 		[0:0]										tin_arst ;

// AXIS INPUT
input 		[0:0]										tin_avalid ;
input 		[255:0]										tin_adata ;
input 		[127:0]										tin_atuser ;

// TUPLE OUTPUT
output 		[0:0]										tin_valid ;
output 		[127:0]										tin_data ;

//#################################
//####     WIRES & REGISTERS
//#################################

//#################################
//####   FINITE STATE MACHINE
//#################################

endmodule // tuser_in_fsm