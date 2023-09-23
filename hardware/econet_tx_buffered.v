module econet_tx_buffered
(
   input          reset,
   input          econet_clk,
   output         econet_data,
   output         transmitting,     // used to turn on transmit line driver
   input          receiving,        // set when the receiver is active to inhibit transmit

   input          sys_clk,
   input [3:0]    sys_we,
   input          sys_select,
   input [9:0]    sys_addr,         // bits [11:2]
   input [31:0]   sys_wdata,
   output [31:0]  sys_rdata,        // Only for registers (buffer doesn't have a system write port)
   input          sys_select_reg
);

   parameter REG_STARTADDR    = 2'b00;     // offset 0
   parameter REG_ENDADDR      = 2'b01;     // offset 4
   parameter REG_CONTROL      = 2'b10;     // offset 8

   parameter ECO_BUFSZ        = 2048;
   parameter ECO_CNTWIDTH     = 11;

   parameter STATE_IDLE       = 3'b000;
   parameter STATE_RXWAIT     = 3'b001;
   parameter STATE_TX_START   = 3'b010;
   parameter STATE_TX         = 3'b011;
   parameter STATE_FCS_1      = 3'b100;
   parameter STATE_FCS_2      = 3'b101;
   parameter STATE_TX_END     = 3'b111;

`ifdef BENCH
   initial begin
      econet_ctr = 0;
      buffer_end = 0;
      state = STATE_IDLE;
      tx_requested = 0;
      $readmemh("test_data_32bit.hex", econet_buf);
   end
`endif

   reg [31:0]              econet_buf[ECO_BUFSZ >> 2];
   reg [31:0]              buf_reg;
   reg [3:0]               idle_counter;
   reg                     turnaround;
   reg [ECO_CNTWIDTH-1:0]  econet_ctr;
   reg [ECO_CNTWIDTH-1:0]  buffer_start;
   reg [ECO_CNTWIDTH-1:0]  buffer_end;
   reg [2:0]               state;
   reg                     tx_requested;
   reg                     start_frame;
   reg                     end_frame;

   wire busy = (state != STATE_IDLE) | transmitting;

   // Econet transmit buffer
   always @(posedge sys_clk) begin
      if(sys_select) begin
         if(sys_we[0])   econet_buf[sys_addr][7:0]     <= sys_wdata[7:0];
         if(sys_we[1])   econet_buf[sys_addr][15:8]    <= sys_wdata[15:8];
         if(sys_we[2])   econet_buf[sys_addr][23:16]   <= sys_wdata[23:16];
         if(sys_we[3])   econet_buf[sys_addr][31:24]   <= sys_wdata[31:24];
      end
   end

   // Transmit registers
   always @(posedge sys_clk) begin
      if(sys_select_reg && sys_we != 0) begin
         case(sys_addr[1:0])
            REG_STARTADDR:
               buffer_start <= sys_wdata[ECO_CNTWIDTH-1:0];
            REG_ENDADDR:
               buffer_end <= sys_wdata[ECO_CNTWIDTH-1:0];
            REG_CONTROL:
               turnaround <= sys_wdata[0];
         endcase
      end
   end
   assign sys_rdata = 
      sys_addr[1:0] == REG_STARTADDR ? 32'h0 | buffer_start :
      sys_addr[1:0] == REG_ENDADDR   ? 32'h0 | buffer_end   :
      sys_addr[1:0] == REG_CONTROL   ? { 29'b0, transmitting, busy, turnaround } :
      32'h55555555;

   // Setting the end offset initiates transmission
   wire tx_req_reset = reset | state != STATE_IDLE;
   always @(posedge sys_clk, posedge tx_req_reset) begin
      if(tx_req_reset)                          
         tx_requested <= 0;
      else if(sys_select_reg && sys_addr[1:0] == REG_ENDADDR && sys_we != 0)   
         tx_requested <= 1;
   end

   // done this way to make sure nextpnr infers block ram
   always @(negedge econet_clk) begin
      buf_reg <= econet_buf[econet_ctr[ECO_CNTWIDTH-1:2]];
   end
   wire [7:0]  econet_byte = econet_ctr[1:0] == 2'b00 ? buf_reg[7:0]    :
                             econet_ctr[1:0] == 2'b01 ? buf_reg[15:8]   :
                             econet_ctr[1:0] == 2'b10 ? buf_reg[23:16]  :
                                                        buf_reg[31:24];

   // Econet spec is to wait 15 bit periods after receive goes low
   // unless we are turning around.
   wire reset_idle = reset | receiving;
   wire line_ready = !receiving && (idle_counter == 4'b1111 || turnaround);
   always @(negedge econet_clk, posedge reset_idle) begin
      if(reset_idle)
         idle_counter <= 0;
      else
         if(idle_counter != 4'b1111) idle_counter <= idle_counter + 1;
   end

   always @(negedge econet_clk, posedge reset) begin
      if(reset) begin
         state <= STATE_IDLE;
         start_frame <= 0;
         end_frame <= 0;
      end
      else begin
         case(state)
            STATE_IDLE: begin
               econet_ctr <= buffer_start;
               start_frame <= 0;
               end_frame <= 0;
               if(tx_requested) begin
                  if(!line_ready) state <= STATE_RXWAIT;
                  else            state <= STATE_TX_START;
               end
            end

            STATE_RXWAIT:
               if(line_ready)     state <= STATE_TX_START;

            STATE_TX_START: begin
               start_frame <= 1;
               state <= STATE_TX;
            end

            STATE_TX: begin
               start_frame <= 0;
               if(increment_ctr) begin
                  if(econet_ctr == buffer_end)  state <= STATE_FCS_1;
                  else                          econet_ctr <= econet_ctr + 1;
               end
            end

            STATE_FCS_1: 
               if(increment_ctr) state <= STATE_FCS_2;

            STATE_FCS_2: begin
               if(increment_ctr) state <= STATE_TX_END;
            end

            STATE_TX_END: begin
               end_frame <= 1;
               if(increment_ctr) state <= STATE_IDLE;
            end

         endcase
      end
   end

   wire increment_ctr;
   wire [7:0] tx_byte = 
      state[2] == 0 ? econet_byte :
      state[0] == 0 ? ~tx_fcs[7:0] :
      state[0] == 1 ? ~tx_fcs[15:8] :
      8'hAA;

   econet_tx transmitter(
      .reset(reset),
      .econet_clk(econet_clk),
      .tx_byte(tx_byte),
      .start_frame(start_frame),
      .end_frame(end_frame),
      .request_byte(increment_ctr),
      .econet_data(econet_data),
      .transmitting(transmitting));

   wire fcs_reset = state == STATE_IDLE;
   wire fcs_strobe = increment_ctr && (state == STATE_TX);
   wire [15:0] tx_fcs;
   fcs transmitter_fcs(
      .reset(fcs_reset),
      .econet_clk(econet_clk),
      .data(econet_byte),
      .enable(fcs_strobe),
      .fcsval(tx_fcs));

endmodule

