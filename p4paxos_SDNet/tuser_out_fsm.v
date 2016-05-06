`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 05/06/2016
// Module Name: tuser_out_fsm
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module tuser_out_fsm (

//#################################
//####       INTERFACES
//#################################

/// CLK & RST
tout_aclk,
tout_arst,

// AXIS INPUT INTERFACE
tout_avalid,
tout_aready,
tout_adata,
tout_akeep,
tout_atlast,

// TUPLE INPUT INTERFACE
tout_valid,
tout_data,

// AXIS OUTPUT INTERFACE
tout_bvalid,
tout_bready,
tout_bdata,
tout_bkeep,
tout_btlast,
tout_btuser,

// DEBUG PORTS
dbg_state

);

//######################################
//####       TYPE OF INTERFACES
//######################################

// CLK & RST
input                                [0:0]                       tin_aclk ;
input                                [0:0]                       tin_arst ;

// AXIS INPUT INTERFACE
input                                [0:0]                       tout_avalid ;
output                        reg    [0:0]                       tout_aready ;
input                                [255:0]                     tout_adata ;
input                                [31:0]                      tout_akeep ;
input                                [0:0]                       tout_atlast ;

// TUPLE INPUT INTERFACE
input                        reg    [0:0]                        tout_valid ;
input                        reg    [127:0]                      tout_data ;

// AXIS OUTPUT INTERFACE
output                        reg    [0:0]                       tout_bvalid ;
input                                [0:0]                       tout_bready ;
output                        reg    [255:0]                     tout_bdata ;
output                        reg    [31:0]                      tout_bkeep ;
output                        reg    [0:0]                       tout_btlast ;
output                               [127:0]                     tout_atuser ;

// DEBUG PORTS
output                               [0:2]                       dbg_state;

//#################################
//####     WIRES & REGISTERS
//#################################

// FSM STATES
reg     [0:2]                   state = 3'bxxx ;
// 000: IDLE
// 001: WRDN

// CONNECTIONS

// DEBUG
assign    dbg_state = state ;

//#################################
//####   FINITE STATE MACHINE
//#################################

always @ ( posedge tout_aclk )

 if (tout_arst == 1)
 
     ////////////////////////// 
     //        RESET
     ////////////////////////// 
     begin

       tout_aready <= 0;
       tout_bvalid <= 0;
       tout_bdata <= 0;
       tout_bkeep <= 0;
       tout_btlast <= 0;
       tout_atuser <= 0;
       
       state <= 3'b000; // IDLE
    
     end

 else begin // begin #2

   case(state)

    ////////////////////////// 
    //    STATE S000: IDLE
    ////////////////////////// 
      3'b000 : begin

          if( tout_avalid == 1 && tout_bready == 1 && tout_valid == 1 )

          begin // IDLE ==> WRDN

            tout_aready <= 1;
            tout_bvalid <= 1;
            tout_bdata <= 0;
            tout_bkeep <= 0;
            tout_btlast <= 0;
            tout_atuser <= 0;
   
            state <= 3'b001; // WRDN

          end

          else

          begin // IDLE ==> IDLE

           tin_aready <= 0;

           tin_bvalid <= 0;
           tin_bdata <= 0;
           tin_bkeep <= 0;
           tin_btlast <= 0;
        
           tin_valid <= 0;
           tin_data <= 0;
   
            state <= 3'b000; // IDLE

          end

           end

    ////////////////////////// 
    //    STATE S001: WRDN
    ////////////////////////// 
      // 3'b001 : begin
       3'b001 : begin

           if( tin_atlast == 1)

           begin // WRDN ==> IDLE

           tin_aready <= 1;

           tin_bvalid <= 1;
           tin_bdata <= tin_adata;
           tin_bkeep <= tin_akeep;
           tin_btlast <= 1;
        
           tin_valid <= 0;
           tin_data <= tin_atuser;
       
          state <= 3'b000; // IDLE

           end

           else

           begin // WRDN ==> WRDN
                     
            tin_aready <= 1;

            tin_bvalid <= 1;
            tin_bdata <= tin_adata;
            tin_bkeep <= tin_akeep;
            tin_btlast <= 0;
            
            tin_valid <= 0;
            tin_data <= tin_atuser;
       
             state <= 3'b001; // WRDN

           end

            end


   endcase

 end // begin #2

endmodule // tuser_in_fsm