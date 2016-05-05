`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: Università della Svizzera italiana
// Engineer: Pietro Bressana
// 
// Create Date: 05/05/2016
// Module Name: tuser_in_tb
// Project Name: SDNet
//////////////////////////////////////////////////////////////////////////////////

module tuser_in_tb // MODULO TESTBENCH

#(
    parameter C_PCI_DATA_WIDTH = 9'd128, // DATA WIDTH DATI PCIe
    parameter BUFF_SIZE = 9'd10 // DIMENSIONE MEMORIA "rBuff"
);

    // CLOCK & RESET
    reg CLK;
    reg RST;
    
    // RICEZIONE  
    wire CHNL_RX_CLK; 
    reg CHNL_RX;     
    wire CHNL_RX_ACK;
    reg CHNL_RX_LAST;
    reg [31:0] CHNL_RX_LEN;
    reg [30:0] CHNL_RX_OFF;
    reg [127:0] CHNL_RX_DATA;
    reg CHNL_RX_DATA_VALID;
    wire CHNL_RX_DATA_REN;
    
    // TRASMISSIONE
    wire CHNL_TX_CLK; 
    wire CHNL_TX; 
    reg CHNL_TX_ACK;
    wire CHNL_TX_LAST;
    wire [31:0] CHNL_TX_LEN;
    wire [30:0] CHNL_TX_OFF;
    wire [127:0] CHNL_TX_DATA;
    wire CHNL_TX_DATA_VALID;
    reg CHNL_TX_DATA_REN;
    
    //////////////////////////////
    // DEBUG CONTENUTO MEMORIA
    wire [127:0] MEM_0;
    wire [127:0] MEM_1;
    wire [127:0] MEM_2;
    wire [127:0] MEM_3;
    wire [127:0] MEM_4;
    wire [127:0] MEM_5;
    wire [127:0] MEM_6;
    wire [127:0] MEM_7;
    wire [127:0] MEM_8;
    wire [127:0] MEM_9;
    //////////////////////////////
    // DEBUG ALTRI SEGNALI
    wire [2:0] STATO_FSM;
    wire [31:0] ITER;
    wire [31:0] CNT;
    wire [31:0] RXLEN;
    wire [31:0] TXLEN;
    //////////////////////////////    
   
// ############################################################################

chnl_tester // ISTANZA UUT (chnl_tester)

#( 
    .C_PCI_DATA_WIDTH(C_PCI_DATA_WIDTH),
    .BUFF_SIZE(BUFF_SIZE)
)   tb_mod (
    
    // CLOCK & RESET
    .CLK                    (CLK),
    .RST                    (RST),
    
    // RICEZIONE
    .CHNL_RX_CLK            (CHNL_RX_CLK), 
    .CHNL_RX                (CHNL_RX), 
    .CHNL_RX_ACK            (CHNL_RX_ACK), 
    .CHNL_RX_LAST           (CHNL_RX_LAST), 
    .CHNL_RX_LEN            (CHNL_RX_LEN), 
    .CHNL_RX_OFF            (CHNL_RX_OFF), 
    .CHNL_RX_DATA           (CHNL_RX_DATA),
    .CHNL_RX_DATA_VALID     (CHNL_RX_DATA_VALID), 
    .CHNL_RX_DATA_REN       (CHNL_RX_DATA_REN),
    
    // TRASMISSIONE
    .CHNL_TX_CLK            (CHNL_TX_CLK), 
    .CHNL_TX                (CHNL_TX), 
    .CHNL_TX_ACK            (CHNL_TX_ACK), 
    .CHNL_TX_LAST           (CHNL_TX_LAST), 
    .CHNL_TX_LEN            (CHNL_TX_LEN), 
    .CHNL_TX_OFF            (CHNL_TX_OFF), 
    .CHNL_TX_DATA           (CHNL_TX_DATA),
    .CHNL_TX_DATA_VALID     (CHNL_TX_DATA_VALID), 
    .CHNL_TX_DATA_REN       (CHNL_TX_DATA_REN),
    
    //////////////////////////////
    // DEBUG CONTENUTO MEMORIA
    .MEM_0                  (MEM_0),
    .MEM_1                  (MEM_1),
    .MEM_2                  (MEM_2),
    .MEM_3                  (MEM_3),
    .MEM_4                  (MEM_4),
    .MEM_5                  (MEM_5),
    .MEM_6                  (MEM_6),
    .MEM_7                  (MEM_7),
    .MEM_8                  (MEM_8),
    .MEM_9                  (MEM_9),
    //////////////////////////////
    // DEBUG ALTRI SEGNALI
    .STATO_FSM               (STATO_FSM),
    .ITER                    (ITER),
    .CNT                     (CNT),
    .RXLEN                   (RXLEN),
    .TXLEN                   (TXLEN)
    //////////////////////////////     
    
    );
 
// ############################################################################ 
    
always#1  CLK=~CLK; // SEGNALE DI CLOCK
      
      initial begin
          
          // VALORI INIZIALI DEI SEGNALI
          CLK <= 0;
          RST <= 0;
          
          CHNL_RX_DATA_VALID <= 0;
          CHNL_RX_DATA <= 997;  
          
          CHNL_TX_ACK <= 0;
          CHNL_TX_DATA_REN <=0;
          
          CHNL_RX <= 1;
          CHNL_RX_LAST <= 1;
          CHNL_RX_LEN <= 40;
          CHNL_RX_OFF <= 0;       
                   
          //////////////////////////////
          // RICEZIONE [100 - 10]          
          //////////////////////////////
          
          #2
          
          while (CHNL_RX_ACK != 1) begin
          #2;// wait
          end           
                      
/*0*/     CHNL_RX_DATA_VALID <= 1;
          CHNL_RX_DATA <= 100;  
          
          while (CHNL_RX_DATA_REN != 1) begin        
          #2;// wait
          end  
  
/*2*/     CHNL_RX_DATA <= 90;

          #2
            
/*3*/     CHNL_RX_DATA <= 80;

          #2
            
/*4*/     CHNL_RX_DATA <= 70;

          #2
            
/*5*/     CHNL_RX_DATA <= 60;

          #2
            
/*6*/     CHNL_RX_DATA <= 50;

          #2
            
/*7*/     CHNL_RX_DATA <= 40;

          #2
            
/*8*/     CHNL_RX_DATA <= 30;

          #2
            
/*9*/     CHNL_RX_DATA <= 20;

          #2
            
/*10*/    CHNL_RX_DATA <= 10;

          #2
          

          CHNL_RX_DATA_VALID <= 0;
          CHNL_RX_DATA <= 997;
          CHNL_RX <= 0;
          CHNL_RX_LAST <= 0;
          CHNL_RX_LEN <= 0;
          CHNL_RX_OFF <= 0;
          
          #2
          
          //////////////////////////////
          // TRASMISSIONE [100 - 10]+3          
          //////////////////////////////
          
          while (CHNL_TX != 1) begin        
          #2;// wait
          end
          
          CHNL_TX_ACK <= 1;          
          
          while (CHNL_TX_DATA_VALID != 1) begin        
          #2;// wait
          end

          CHNL_TX_ACK <= 0;     
          CHNL_TX_DATA_REN <=1;
          
          while (CHNL_TX_DATA_VALID == 1) begin        
          #2;// wait
          end
          
          CHNL_TX_DATA_REN <=0; 
          
          #2
          
          //////////////////////////////
          // RICEZIONE [1 - 10]          
          //////////////////////////////
          
          CHNL_RX <= 1;
          CHNL_RX_LAST <= 1;
          CHNL_RX_LEN <= 40;
          CHNL_RX_OFF <= 0; 
         
          #2
          
          while (CHNL_RX_ACK != 1) begin        
          #2;// wait
          end
 
/*0*/     CHNL_RX_DATA_VALID <= 1;
          CHNL_RX_DATA <= 1;        
                    
          while (CHNL_RX_DATA_REN != 1) begin         
          #2;// wait
          end  
  
/*2*/     CHNL_RX_DATA <= 2;

          #2
            
/*3*/     CHNL_RX_DATA <= 3;

          #2
            
/*4*/     CHNL_RX_DATA <= 4;

          #2
            
/*5*/     CHNL_RX_DATA <= 5;

          #2
            
/*6*/     CHNL_RX_DATA <= 6;

          #2
            
/*7*/     CHNL_RX_DATA <= 7;

          #2
            
/*8*/     CHNL_RX_DATA <= 8;

          #2
            
/*9*/     CHNL_RX_DATA <= 9;

          #2
            
/*10*/    CHNL_RX_DATA <= 10;

           #2

          CHNL_RX_DATA_VALID <= 0;
          CHNL_RX_DATA <= 997;
          CHNL_RX <= 0;
          CHNL_RX_LAST <= 0;
          CHNL_RX_LEN <= 0;
          CHNL_RX_OFF <= 0;
          
           #2
                   
           //////////////////////////////
           // TRASMISSIONE [1 - 10]+3          
           //////////////////////////////
           
           while (CHNL_TX != 1) begin        
           #2;// wait
           end
           
           CHNL_TX_ACK <= 1;          
           
           while (CHNL_TX_DATA_VALID != 1) begin        
           #2;// wait
           end
 
           CHNL_TX_ACK <= 0;     
           CHNL_TX_DATA_REN <=1;
           
           while (CHNL_TX_DATA_VALID == 1) begin        
           #2;// wait
           end
           
           CHNL_TX_DATA_REN <=0; 
           
           #2;       
           
      end
  
endmodule