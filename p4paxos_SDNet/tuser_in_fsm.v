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
//tin_adata,
tin_atuser,
tin_atlast,

// TUPLE OUTPUT
tin_valid,
tin_data

);

//######################################
//####       TYPE OF INTERFACES
//######################################


// CLK & RST
input     [0:0]                   tin_aclk ;
input     [0:0]                   tin_arst ;

// AXIS INPUT
input     [0:0]                   tin_avalid ;
//input     [255:0]                   tin_adata ;
input     [127:0]                   tin_atuser ;
input     [0:0]                   tin_atlast ;

// TUPLE OUTPUT
output reg  [0:0]                   tin_valid ;
output reg  [127:0]                   tin_data ;

//#################################
//####     WIRES & REGISTERS
//#################################

// FSM STATES
reg     [0:0]                   state ;

//#################################
//####   FINITE STATE MACHINE
//#################################

always @ ( posedge tin_aclk )

 if (tin_arst == 1'b1)
 
     // RESET
     begin
    
       tin_valid <= 1'b0;
       tin_data <= 128'b0;
       
       state <= 0; // IDLE
    
     end

 else begin // begin #2

   case(state)

        // STATE S0: IDLE
      0 : begin

          if( tin_avalid == 1'b1 )

          begin // IDLE ==> READY

            tin_valid <= 1'b1;
            tin_data <= tin_atuser;
   
            state <= 1; // READY

          end

          else

          begin // IDLE ==> IDLE

            tin_valid <= 1'b0;
            tin_data <= 128'b0;
   
            state <= 0; // IDLE

          end

           end

        // STATE S1: READY
      1 : begin

          if( tin_avalid == 1'b0 || tin_atlast == 1'b1 )

          begin // READY ==> IDLE

            tin_valid <= 1'b0;
            tin_data <= 128'b0;
   
            state <= 0; // IDLE

          end

          else

          begin // READY ==> READY
                    
            tin_valid <= 1'b1;
            tin_data <= tin_atuser;
   
            state <= 1; // READY

          end

           end

   endcase

 end // begin #2

endmodule // tuser_in_fsm