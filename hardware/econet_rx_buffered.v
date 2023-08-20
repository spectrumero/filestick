module buffered_econet
(
   input          reset,

   input          econet_clk,
   input          econet_rx,

   input          sys_clk,
   input          sys_rd,
   input          sys_select,
   input [7:0]    sys_addr,         // 32-bit addr [9:2]
   output [31:0]  sys_data,
   output [31:0]  sys_frame_start,
   output [31:0]  sys_frame_end,
   output         sys_frame_valid
);

   parameter      ECO_BUFSZ = 512;
   parameter      ECO_CNTWIDTH = 9;
   parameter      FCS_GOOD = 16'hF0B8;

   reg [31:0]           econet_buf[ECO_BUFSZ >> 2];
   reg [31:0]           sys_data;
   reg                  sys_frame_valid;

   reg [ECO_CNTWIDTH-1:0] econet_ctr;
   reg [ECO_CNTWIDTH-1:0] frame_start;
   reg [ECO_CNTWIDTH-1:0] frame_end;

   assign sys_frame_start  = 32'b0 | frame_start;
   assign sys_frame_end    = 32'b0 | frame_end;

   wire [7:0]           rx_byte;
   wire [15:0]          rx_fcs;
   wire                 rx_byte_ready;
   wire                 rx_frame_start;
   wire                 rx_frame_end;

`ifdef BENCH
   initial begin
      econet_ctr = 0;
      frame_start = 0;
      frame_end = 0;
      sys_frame_valid = 0;
      sys_data = 0;
   end
`endif

   always @(posedge econet_clk) begin
      if(rx_frame_start)         frame_start <= econet_ctr;
      else if(rx_frame_end)      frame_end   <= econet_ctr;
   end

   // system select acts as an async reset for the frame_valid flag (which is intended to be used to trigger
   // an interrupt)
   always @(posedge econet_clk, posedge sys_select) begin
      if(sys_select)
         sys_frame_valid <= 0;
      else
         if(rx_frame_end && rx_fcs == FCS_GOOD) sys_frame_valid <= 1;
   end
         

   always @(posedge econet_clk) begin
      if(rx_byte_ready) begin
         case(econet_ctr & 2'b11) 
            0:       econet_buf[econet_ctr[ECO_CNTWIDTH-1:2]][7:0] <= rx_byte;
            1:       econet_buf[econet_ctr[ECO_CNTWIDTH-1:2]][15:8] <= rx_byte;
            2:       econet_buf[econet_ctr[ECO_CNTWIDTH-1:2]][23:16] <= rx_byte;
            3:       econet_buf[econet_ctr[ECO_CNTWIDTH-1:2]][31:24] <= rx_byte;
         endcase

         econet_ctr <= econet_ctr + 1;
      end
   end

   always @(posedge sys_clk) begin
      if(sys_rd & sys_select) sys_data <= econet_buf[sys_addr];
   end

   econet   econet_receiver(
      .reset(reset),
      .econet_clk(econet_clk),
      .rx(econet_rx),

      .rx_byte(rx_byte),
      .rx_fcs(rx_fcs),
      .rx_byte_ready(rx_byte_ready),
      .rx_frame_start(rx_frame_start),
      .rx_frame_end(rx_frame_end));

endmodule
