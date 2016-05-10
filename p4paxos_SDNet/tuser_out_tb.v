`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 05/06/2016
// Module Name: tuser_out_tb
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module tuser_out_tb();

       // CLOCK & RESET
       reg                         clk;
       reg                         rst;
       
       // AXIS INPUT INTERFACE
       reg                         tout_avalid ;
       wire                        tout_aready ;
       reg    [255:0]              tout_adata ;
       reg    [31:0]               tout_akeep ;
       reg                         tout_atlast ;

        // TUPLE INPUT INTERFACE
       reg                         tout_valid ;
       reg    [127:0]              tout_data ;
       

       // AXIS OUTPUT INTERFACE
       wire                        tout_bvalid ;
       reg                         tout_bready ;
       wire   [255:0]              tout_bdata ;
       wire   [31:0]               tout_bkeep ;
       wire                        tout_btlast ;
       wire   [127:0]              tout_btuser ;

       // DEBUG PORTS
       wire   [0:2]                dbg_state;
       
       // ITERATION VARIABLE
       reg    [0:3]                i = 0;
      
   // ############################################################################

   tuser_out_fsm  tuser_out_fsm_inst// UUT

   (

   // CLK & RST
   .tout_aclk                                 (clk),
   .tout_arst                                 (rst),

   // AXIS INPUT INTERFACE
   .tout_avalid                               (tout_avalid),
   .tout_aready                               (tout_aready),
   .tout_adata                                (tout_adata),
   .tout_akeep                                (tout_akeep),
   .tout_atlast                               (tout_atlast),

   // TUPLE INPUT INTERFACE
   .tout_valid                                (tout_valid),
   .tout_data                                 (tout_data),
   
   // AXIS OUTPUT INTERFACE
   .tout_bvalid                               (tout_bvalid),
   .tout_bready                               (tout_bready),
   .tout_bdata                                (tout_bdata),
   .tout_bkeep                                (tout_bkeep),
   .tout_btlast                               (tout_btlast),
   .tout_btuser                               (tout_btuser),

   // DEBUG PORTS
   .dbg_state                                 (dbg_state)

   );

   // ############################################################################ 
       
   always #1 clk=~clk; // CLOCK SIGNAL
         
         initial begin
             
             // INITIAL VALUES
    
             clk <= 0;
             rst <= 0; 
            
             tout_avalid <= 0;
             tout_adata <= 256'b0;//256
             tout_akeep <= 32'b0;//32
             tout_atlast <= 0;
             tout_valid <= 0;
             tout_data <= 128'b0;//128
             tout_bready <= 0;

             // INITIAL RESET
          
             rst <= 1;

             #4

             rst <= 0;
    
             while (i < 10) begin                    
                      
             //////////////////////////////
             // PACKET          
             //////////////////////////////
             
             #4

             tout_avalid <= 0;
             tout_adata <= 22222;//256
             tout_akeep <= 33333;//32
             tout_atlast <= 0;
             tout_valid <= 1;
             tout_data <= 44444;//128
             tout_bready <= 1;

             #2
             
             tout_avalid <= 1;
             tout_adata <= 22222;//256
             tout_akeep <= 33333;//32
             tout_atlast <= 0;
             tout_valid <= 1;
             tout_data <= 44444;//128
             tout_bready <= 1;
             
             #1

             while (tout_aready != 1) begin
             #2;// wait
             end 

             tout_avalid <= 1;
             tout_adata <= 22222;//256
             tout_akeep <= 33333;//32
             tout_atlast <= 0;
             tout_valid <= 0;
             tout_data <= 44444;//128
             tout_bready <= 1; 

             #3

             tout_avalid <= 1;
             tout_adata <= 22222;//256
             tout_akeep <= 33333;//32
             tout_atlast <= 1;
             tout_valid <= 0;
             tout_data <= 44444;//128
             tout_bready <= 1; 

             #2
             
             //////////////////////////////
             // EMPTY PACKET          
             //////////////////////////////

             tout_avalid <= 0;
             tout_adata <= 0;//256
             tout_akeep <= 0;//32
             tout_atlast <= 0;
             tout_valid <= 0;
             tout_data <= 0;//128
             tout_bready <= 0;

             // INCREMENT ITERATION VARIABLE
             i <= i + 1 ; 
             
             end // while
                
         end // INITIAL
     
   endmodule