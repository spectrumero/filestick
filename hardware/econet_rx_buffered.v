// Econet buffered receiver.
// Receives data into a circular buffer. If the station/net numbers match the
// station/net that was set, and the FCS is valid, the sys_frame_valid goes
// high. This is used to cause a CPU interrupt.
// Reading from the buffer resets the sys_frame_valid flag.

module buffered_econet
(
   input          reset,            // global reset

   input          econet_clk,       // econet clock
   input          econet_rx,        // econet input
   input          inhibit,          // set to prevent receiving

   input          sys_clk,          // CPU clock
   input          sys_rd,           // CPU read
   input [3:0]    sys_wr,           // CPU write
   input          sys_buf_select,   // Buffer select (sys_rdata set to buffer at sys_addr)
   input          sys_reg_select,   // Register select (sys_rdata set to register values)
   input [31:0]   sys_wdata,        // CPU write data (address set only)
   input [7:0]    sys_addr,         // 32-bit addr [9:2]
   output [31:0]  sys_rdata,        // CPU read data
   output         sys_frame_valid,  // true when a valid frame is received
   output         receiving         // true while receiving
);

   // Register addresses
   parameter      REG_START_PTR        = 0;     // 0x00
   parameter      REG_END_PTR          = 1;     // 0x04
   parameter      REG_BYTE_COUNT       = 2;     // 0x08
   parameter      REG_ADDRESS          = 3;     // 0x0C
   parameter      REG_REPLY_ADDRESS    = 4;     // 0x10
   parameter      REG_SCOUT_DATA       = 5;     // 0x14
   parameter      REG_OUR_ADDRESS      = 6;     // 0x18
   parameter      REG_STATUS           = 7;     // 0x1C
   parameter      REG_STATUS_PEEK      = 8;     // 0x20

   parameter      ECO_BUFSZ = 512;
   parameter      ECO_CNTWIDTH = 9;
   parameter      FCS_GOOD = 16'hF0B8;

   reg [31:0]           econet_buf[ECO_BUFSZ >> 2];
   reg [31:0]           buf_data;
   reg                  sys_frame_valid;
   reg [15:0]           econet_address;

   reg [ECO_CNTWIDTH-1:0] econet_ptr;
   reg [ECO_CNTWIDTH-1:0] econet_ctr;
   reg [ECO_CNTWIDTH-1:0] frame_start;
   reg [7:0]            frame_address[6];
   reg                  frame_state;

   // These are set when a frame with a valid FCS for our econet address
   // is received. Storing values here will mean software has more
   // time (until another complete valid frame for our address) is
   // received to process them.
   reg [ECO_CNTWIDTH-1:0]  valid_start;      
   reg [ECO_CNTWIDTH-1:0]  valid_end;
   reg [ECO_CNTWIDTH-1:0]  valid_cnt;
   reg [31:0]              valid_address;
   reg [15:0]              valid_scout;

   parameter      FRAME_STATE_IDLE = 0;
   parameter      FRAME_STATE_RX   = 1;

   wire [7:0]           rx_byte;
   wire [15:0]          rx_fcs;
   wire                 rx_byte_ready;
   wire                 rx_frame_start;
   wire                 rx_frame_end;

`ifdef BENCH
   initial begin
      buf_data = 0;
      econet_ctr = 0;
      econet_ptr = 0;
      frame_start = 0;
      sys_frame_valid = 0;
      frame_state = 0;
      frame_address[0] = 0;
      frame_address[1] = 0;
      frame_address[2] = 0;
      frame_address[3] = 0;
      frame_address[4] = 0;
      frame_address[5] = 0;
   end

   wire [31:0] fr_address;
   wire [15:0] sc_data;
   assign fr_address[7:0] = frame_address[0];
   assign fr_address[15:8] = frame_address[1];
   assign fr_address[23:16] = frame_address[2];
   assign fr_address[31:24] = frame_address[3];
   assign sc_data[15:8] = frame_address[4];
   assign sc_data[7:0]  = frame_address[5];
`endif

   always @(posedge econet_clk) begin
      if(rx_frame_start)         frame_start <= econet_ptr;
   end

   // Extract the station/net from the frame
   always @(posedge econet_clk, posedge rx_frame_end) begin
      if(rx_frame_end)
         frame_state <= FRAME_STATE_IDLE;
      else begin
         case(frame_state)
            FRAME_STATE_IDLE:
               if(rx_frame_start) begin
                  frame_state <= FRAME_STATE_RX;
               end

               FRAME_STATE_RX: begin
                  if(econet_ctr < 6) frame_address[econet_ctr] <= rx_byte;
               end
         endcase
      end
   end

   // register select acts as an async reset for the frame_valid flag (which is intended to be used to trigger
   // an interrupt)
   always @(posedge econet_clk, posedge valid_rst) begin
      if(valid_rst)
         sys_frame_valid <= 0;
      else
         // Frame for our address received and is valid
         if(rx_frame_end && rx_fcs == FCS_GOOD && 
            econet_address[7:0] == frame_address[0] && econet_address[15:8] == frame_address[1]) begin
            valid_start          <= frame_start;
            valid_end            <= econet_ptr;
            valid_cnt            <= econet_ctr;
            valid_address[7:0]   <= frame_address[0];
            valid_address[15:8]  <= frame_address[1];
            valid_address[23:16] <= frame_address[2];
            valid_address[31:24] <= frame_address[3];
            valid_scout[15:8]    <= frame_address[4]; // scout flags
            valid_scout[7:0]     <= frame_address[5]; // scout port
            sys_frame_valid      <= 1;
         end
   end
         

   always @(posedge econet_clk) begin
      if(rx_frame_start) econet_ctr <= 0;
      else if(rx_byte_ready) econet_ctr <= econet_ctr + 1;

      if(rx_byte_ready) begin
         case(econet_ptr & 2'b11) 
            0:       econet_buf[econet_ptr[ECO_CNTWIDTH-1:2]][7:0] <= rx_byte;
            1:       econet_buf[econet_ptr[ECO_CNTWIDTH-1:2]][15:8] <= rx_byte;
            2:       econet_buf[econet_ptr[ECO_CNTWIDTH-1:2]][23:16] <= rx_byte;
            3:       econet_buf[econet_ptr[ECO_CNTWIDTH-1:2]][31:24] <= rx_byte;
         endcase

         econet_ptr <= econet_ptr + 1;
      end
   end

   always @(posedge sys_clk)
      if(sys_rd & sys_buf_select) buf_data <= econet_buf[sys_addr];

   assign sys_rdata =
      sys_buf_select ? buf_data : reg_data;

   wire [3:0] sys_reg_addr;
   assign sys_reg_addr = sys_addr[3:0];
   wire [31:0] reg_data =
      sys_reg_addr == REG_START_PTR       ? 32'b0 | valid_start :
      sys_reg_addr == REG_END_PTR         ? 32'b0 | valid_end :
      sys_reg_addr == REG_BYTE_COUNT      ? 32'b0 | valid_cnt :
      sys_reg_addr == REG_ADDRESS         ? 32'b0 | valid_address :
      sys_reg_addr == REG_OUR_ADDRESS     ? { 16'b0, econet_address } :
      sys_reg_addr == REG_REPLY_ADDRESS   ? { valid_address[15:8], valid_address[7:0], valid_address[31:24], valid_address[23:16] } :
      sys_reg_addr == REG_SCOUT_DATA      ? { 16'b0, valid_scout } :
      sys_reg_addr == REG_STATUS          ? { period, 13'b0, clk_detected, receiving, sys_frame_valid } :
      sys_reg_addr == REG_STATUS_PEEK     ? { period, 13'b0, clk_detected, receiving, sys_frame_valid } :
      32'h55555555;

   always @(posedge sys_clk) begin
      if(sys_wr & sys_reg_select) begin
         if(sys_reg_addr == REG_OUR_ADDRESS) begin
            if(sys_wr[0]) econet_address[7:0] <= sys_wdata[7:0];
            if(sys_wr[1]) econet_address[15:8] <= sys_wdata[15:8];
         end
      end
   end

   // Resets the 'valid' flag, this must be done with a delay so that
   // the CPU can actually read it, as the action of reading the RX
   // registers is what causes the 'valid' flag to get reset. This flag
   // is used to generate the data received interrupt.
   reg [1:0] valid_rst_state;
   reg valid_rst;
   always @(posedge sys_clk, posedge reset) begin
      if(reset) begin
         valid_rst <= 0;
         valid_rst_state <= 0;
      end
      else begin
         case(valid_rst_state)
            0: 
               if((sys_reg_select & sys_rd) && sys_reg_addr == REG_STATUS)
                  valid_rst_state <= 1;
            1: valid_rst_state <= 2;
            2: begin
               valid_rst <= 1;
               valid_rst_state <= 3;
            end
            3: begin
               valid_rst <= 0;
               valid_rst_state <= 0;
            end
         endcase
      end
   end

   // Measure the econet clock period
   reg [15:0] period_cnt;
   reg [15:0] period;
   reg [1:0]  period_cnt_state;
   always @(posedge sys_clk, posedge reset) begin
      if(reset) begin
         period_cnt <= 0;
         period_cnt_state <= 0;
         period <= 0;
      end
      else begin
         case(period_cnt_state)
            0: begin
               if(econet_clk) period_cnt_state <= 1;
               period_cnt <= period_cnt + 1;
            end
            1: begin
               if(!econet_clk) period_cnt_state <= 2;
               period_cnt <= period_cnt + 1;
            end
            2: begin
               if(econet_clk) begin
                  period_cnt_state <= 3;
                  period <= period_cnt;
               end
               period_cnt <= period_cnt + 1;
            end
            3: begin
               period_cnt_state <= 0;
               period_cnt <= 0;
            end
         endcase
      end
   end

   // Set the clock detected flag
   reg clk_detected;
   wire clk_det = period_cnt_state == 3;
   always @(posedge sys_clk, posedge clk_det) begin
      if(clk_det)
         clk_detected <= 1;
      else
         if(period_cnt == 16'hFFFF) clk_detected <= 0;
   end
         

   econet   econet_receiver(
      .reset(reset),
      .econet_clk(econet_clk),
      .rx(econet_rx),
      .inhibit(inhibit),

      .rx_byte(rx_byte),
      .rx_fcs(rx_fcs),
      .rx_byte_ready(rx_byte_ready),
      .rx_frame_start(rx_frame_start),
      .rx_frame_end(rx_frame_end),
   
      .receiving(receiving));

endmodule
