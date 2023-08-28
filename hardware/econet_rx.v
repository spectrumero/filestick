//
module econet
(
   input reset,
   input econet_clk,
   input rx,

   output [7:0]   rx_byte,
   output [15:0]  rx_fcs,
   output         rx_byte_ready,
   output         rx_frame_start,
   output         rx_frame_end,
   output         receiving
);

   wire rx_byte_ready;
   wire rx_frame_start;
   wire rx_frame_end;
   wire [7:0] rx_byte;
   wire [15:0] rx_fcs;

   wire reset_fcs;
   wire [7:0] fcs_data;
   wire fcs_enable;

   assign reset_fcs = rx_frame_start;
   assign fcs_data = rx_byte;
   assign fcs_enable = rx_byte_ready;

   // Receive frames
   econet_rx receiver(
      .reset(reset),
      .econet_clk(econet_clk),
      .econet_data(rx),
      .data_out(rx_byte),
      .data_strobe(rx_byte_ready),
      .frame_start(rx_frame_start),
      .frame_end(rx_frame_end),
      .receiving(receiving)
   );

   fcs econet_fcs(
      .reset(reset_fcs),
      .econet_clk(econet_clk),
      .data(fcs_data),
      .enable(fcs_enable),
      .fcsval(rx_fcs)
   );


endmodule
