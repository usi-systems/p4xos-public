`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 05/05/2016
// Module Name: tuser_in_tb
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module tuser_in_tb();

    // CLOCK & RESET
    reg                         clk;
    reg                         rst;
    
    // AXIS INPUT INTERFACE
    reg                         tin_avalid ;
    wire                        tin_aready ;
    reg [255:0]                 tin_adata ;
    reg [31:0]                  tin_akeep ;
    reg                         tin_atlast ;
    reg [127:0]                 tin_atuser ;

    // AXIS OUTPUT INTERFACE
    wire                        tin_bvalid ;
    reg                         tin_bready ;
    wire[255:0]                 tin_bdata ;
    wire[31:0]                  tin_bkeep ;
    wire                        tin_btlast ;

    // TUPLE OUTPUT INTERFACE
    wire                        tin_valid ;
    wire  [127:0]               tin_data ;

    // DEBUG PORTS
    wire  [0:2]                 dbg_state;
   
// ############################################################################

tuser_in_fsm  tuser_in_fsm_inst// UUT

(

// CLK & RST
.tin_aclk                                 (clk),
.tin_arst                                 (rst),

// AXIS INPUT INTERFACE
.tin_avalid                               (tin_avalid),
.tin_aready                               (tin_aready),
.tin_adata                                (tin_adata),
.tin_akeep                                (tin_akeep),
.tin_atlast                               (tin_atlast),
.tin_atuser                               (tin_atuser),

// AXIS OUTPUT INTERFACE
.tin_bvalid                               (tin_bvalid),
.tin_bready                               (tin_bready),
.tin_bdata                                (tin_bdata),
.tin_bkeep                                (tin_bkeep),
.tin_btlast                               (tin_btlast),

// TUPLE OUTPUT INTERFACE
.tin_valid                                (tin_valid),
.tin_data                                 (tin_data),

// DEBUG PORTS
.dbg_state                                (dbg_state)

);

// ############################################################################ 
    
always#1  clk=~clk; // CLOCK SIGNAL
      
      initial begin
          
          // INITIAL VALUES
 
          clk <= 0;
          rst <= 0; 
 
          tin_avalid <= 0;
          tin_adata <= 256'b0;//256
          tin_akeep <= 32'b0;//32
          tin_atlast <= 0;
          tin_atuser <= 128'b0;//128
          tin_bready <= 0;

          // INITIAL RESET
       
          rst <= 1;

          #10

          rst <= 0;
                   
          //////////////////////////////
          // PACKET          
          //////////////////////////////
          
          #2

          tin_avalid <= 0;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 0;
          tin_atuser <= 44444;//128
          tin_bready <= 1;

          #2
          
          tin_avalid <= 1;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 0;
          tin_atuser <= 44444;//128
          tin_bready <= 1;

          while (tin_aready != 1) begin
          #2;// wait
          end 

          tin_avalid <= 1;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 0;
          tin_atuser <= 44444;//128
          tin_bready <= 1;

          #2

          tin_avalid <= 1;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 1;
          tin_atuser <= 44444;//128
          tin_bready <= 1;
          
          #2
          
          //////////////////////////////
          // EMPTY PACKET          
          //////////////////////////////
          
          tin_avalid <= 0;
          tin_adata <= 0;//256
          tin_akeep <= 0;//32
          tin_atlast <= 0;
          tin_atuser <= 0;//128
          tin_bready <= 0;
          
          //////////////////////////////
          // PACKET          
          //////////////////////////////
          
          #8

          tin_avalid <= 0;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 0;
          tin_atuser <= 44444;//128
          tin_bready <= 1;

          #2
          
          tin_avalid <= 1;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 0;
          tin_atuser <= 44444;//128
          tin_bready <= 1;

          while (tin_aready != 1) begin
          #2;// wait
          end 

          tin_avalid <= 1;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 0;
          tin_atuser <= 44444;//128
          tin_bready <= 1;

          #2

          tin_avalid <= 1;
          tin_adata <= 22222;//256
          tin_akeep <= 33333;//32
          tin_atlast <= 1;
          tin_atuser <= 44444;//128
          tin_bready <= 1;
          
          #2
          
          //////////////////////////////
          // EMPTY PACKET          
          //////////////////////////////
                    
          tin_avalid <= 0;
          tin_adata <= 0;//256
          tin_akeep <= 0;//32
          tin_atlast <= 0;
          tin_atuser <= 0;//128
          tin_bready <= 0;

            
      end // INITIAL
  
endmodule