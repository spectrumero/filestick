module econet_tx_buffered
(
   input          reset,
   input          econet_clk,
   output         econet_data,
   output         busy,             // set whenever there's an active buffer
   output         transmitting,     // used to turn on transmit line driver
   input          receiving,        // set when the receiver is active to inhibit transmit

   input          sys_clk,
   input [3:0]    sys_we,
   input          sys_select,
   input [7:0]    sys_addr,         // bits [9:2]
   input [31:0]   sys_data,
   input          sys_select_frame_start,
   input          sys_select_buffer_end
);

   parameter ECO_BUFSZ        = 512;
   parameter ECO_CNTWIDTH     = 9;

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
   reg [ECO_CNTWIDTH-1:0]  econet_ctr;
   reg [ECO_CNTWIDTH-1:0]  buffer_start;
   reg [ECO_CNTWIDTH-1:0]  buffer_end;
   reg [2:0]               state;
   reg                     tx_requested;
   reg                     start_frame;
   reg                     end_frame;

   wire busy = (state != STATE_IDLE) | transmitting;

   always @(posedge sys_clk) begin
      if(sys_select) begin
         if(sys_we[0])   econet_buf[sys_addr][7:0]     <= sys_data[7:0];
         if(sys_we[1])   econet_buf[sys_addr][15:8]    <= sys_data[15:8];
         if(sys_we[2])   econet_buf[sys_addr][23:16]   <= sys_data[23:16];
         if(sys_we[3])   econet_buf[sys_addr][31:24]   <= sys_data[31:24];
      end
      else if(sys_select_frame_start && sys_we != 0)
         buffer_start  <= sys_data[ECO_CNTWIDTH-1:0];
      else if(sys_select_buffer_end && sys_we != 0) 
         buffer_end   <= sys_data[ECO_CNTWIDTH-1:0];
   end

   wire tx_req_reset = reset | state != STATE_IDLE;
   always @(posedge sys_clk, posedge tx_req_reset) begin
      if(tx_req_reset)                          tx_requested <= 0;
      else if(sys_select_buffer_end && sys_we != 0)   tx_requested <= 1;
   end

   // done this way to make sure nextpnr infers block ram
   always @(posedge econet_clk) begin
      buf_reg <= econet_buf[econet_ctr[ECO_CNTWIDTH-1:2]];
   end
   wire [7:0]  econet_byte = econet_ctr[1:0] == 2'b00 ? buf_reg[7:0]    :
                             econet_ctr[1:0] == 2'b01 ? buf_reg[15:8]   :
                             econet_ctr[1:0] == 2'b10 ? buf_reg[23:16]  :
                                                        buf_reg[31:24];

   always @(posedge econet_clk, posedge reset) begin
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
                  if(receiving)  state <= STATE_RXWAIT;
                  else           state <= STATE_TX_START;
               end
            end

            STATE_RXWAIT:
               if(!receiving)    state <= STATE_TX_START;

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

