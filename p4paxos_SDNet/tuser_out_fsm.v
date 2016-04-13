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

);

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
output reg  [127:0]										tout_atuser ;

//#################################
//####     WIRES & REGISTERS
//#################################

// FSM STATES
reg 		[0:0]										state ;

//#################################
//####   FINITE STATE MACHINE
//#################################

always @ ( posedge tout_aclk )

 if (tout_arst == 1'b1)
 
     // RESET
     begin
    
       tout_atuser <= 128'b0;
       
       state <= 0; // IDLE
    
     end

 else begin // begin #2

   case(state)

        // STATE S0: IDLE
    	0 : begin

    			if( tout_avalid == 1'b1 && tout_valid == 1'b1 )

    			begin // IDLE ==> READY

                    tout_atuser <= tout_data;
   
    				state <= 1; // READY

    			end

    			else

    			begin // IDLE ==> IDLE

                    tout_atuser <= 128'b0;
   
    				state <= 0; // IDLE

    			end

           end

        // STATE S1: READY
    	1 : begin

    			if( tout_avalid == 1'b0 || tout_valid == 1'b0 )

    			begin // READY ==> IDLE

                    tout_atuser <= 128'b0;
   
    				state <= 0; // IDLE

    			end

    			else

    			begin // READY ==> READY
                    
                    tout_atuser <= tout_data;
   
    				state <= 1; // READY

    			end

           end

   endcase

 end // begin #2
endmodule // tuser_out_fsm