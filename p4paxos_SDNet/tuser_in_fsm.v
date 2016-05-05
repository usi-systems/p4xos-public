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

// AXIS INPUT INTERFACE
tin_avalid,
tin_aready,
tin_adata,
tin_akeep,
tin_atlast,
tin_atuser,

// AXIS OUTPUT INTERFACE
tin_bvalid,
tin_bready,
tin_bdata,
tin_bkeep,
tin_btlast,

// TUPLE OUTPUT INTERFACE
tin_valid,
tin_data

);

//######################################
//####       TYPE OF INTERFACES
//######################################

// CLK & RST
input     [0:0]                   tin_aclk ;
input     [0:0]                   tin_arst ;

// AXIS INPUT INTERFACE
input     [0:0]                   tin_avalid ;
output    [0:0]                   tin_aready ;
input     [255:0]                 tin_adata ;
input     [31:0]                  tin_akeep ;
input     [0:0]                   tin_atlast ;
input     [127:0]                 tin_atuser ;

// AXIS OUTPUT INTERFACE
output    [0:0]                   tin_bvalid ;
input     [0:0]                   tin_bready ;
output    [255:0]                 tin_bdata ;
output    [31:0]                  tin_bkeep ;
output    [0:0]                   tin_btlast ;

// TUPLE OUTPUT INTERFACE
output reg  [0:0]                 tin_valid ;
output reg  [127:0]               tin_data ;

//#################################
//####     WIRES & REGISTERS
//#################################

// FSM STATES
reg     [0:2]                   state ;
// 000: IDLE
// 001: WRD1
// 010: WRDN

//#################################
//####   FINITE STATE MACHINE
//#################################

always @ ( posedge tin_aclk )

 if (tin_arst == 1)
 
     ////////////////////////// 
     //        RESET
     ////////////////////////// 
     begin

       tin_aready <== 0;

       tin_bvalid <== 0;
       tin_bdata <== 0;
       tin_bkeep <== 0;
       tin_btlast <== 0;
    
       tin_valid <= 0;
       tin_data <= 0;
       
       state <= 0; // IDLE
    
     end

 else begin // begin #2

   case(state)

    ////////////////////////// 
    //    STATE S000: IDLE
    ////////////////////////// 
      3'b000 : begin

          if( tin_avalid == 1 && tin_bready == 1 )

          begin // IDLE ==> WRD1

           tin_aready <== 1;

           tin_bvalid <== 1;
           tin_bdata <== tin_adata;
           tin_bkeep <== tin_akeep;
           tin_btlast <== tin_atlast;
        
           tin_valid <= 0;
           tin_data <= tin_atuser;
   
            state <= 3'b001; // WRD1

          end

          else

          begin // IDLE ==> IDLE

           tin_aready <== 0;

           tin_bvalid <== 0;
           tin_bdata <== 0;
           tin_bkeep <== 0;
           tin_btlast <== 0;
        
           tin_valid <= 0;
           tin_data <= 0;
   
            state <= 3'b000; // IDLE

          end

           end

    ////////////////////////// 
    //    STATE S001: WRD1
    //////////////////////////
      3'b001 : begin

            tin_aready <== 1;

            tin_bvalid <== 1;
            tin_bdata <== tin_adata;
            tin_bkeep <== tin_akeep;
            tin_btlast <== tin_atlast;
            
            tin_valid <= 1;
            tin_data <= tin_atuser;
   
            state <= 3'b010; // WRDN

           end

    ////////////////////////// 
    //    STATE S010: WRDN
    ////////////////////////// 
       3'b010 : begin

           if( tin_atlast == 1)

           begin // WRDN ==> IDLE

           tin_aready <== 0;

           tin_bvalid <== 0;
           tin_bdata <== 0;
           tin_bkeep <== 0;
           tin_btlast <== 0;
        
           tin_valid <= 0;
           tin_data <= 0;
       
          state <= 3'b000; // IDLE

           end

           else

           begin // WRDN ==> WRDN
                     
            tin_aready <== 1;

            tin_bvalid <== 1;
            tin_bdata <== tin_adata;
            tin_bkeep <== tin_akeep;
            tin_btlast <== tin_atlast;
            
            tin_valid <= 0;
            tin_data <= tin_atuser;
       
             state <= 3'b010; // IDLE

           end

            end

   endcase

 end // begin #2

endmodule // tuser_in_fsm