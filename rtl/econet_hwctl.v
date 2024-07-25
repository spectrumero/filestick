// Econet misc hardware control
module econet_hwctl
(
   input          reset,
   input          clk,
   input [3:0]    wr,
   input          select,
   input [31:0]   data_in,
   output [31:0]  data_out,

   input          collision_detect,
   output         econet_clken,
   output         econet_clkout,
   output         econet_termen,
   output         coldet_interrupt
);

reg         econet_clken;     // Clock output enable
reg         econet_termen;    // Terminator enable
reg [2:0]   clkdiv;           // Clock divider setting

assign      data_out = { 22'b0, clkdiv, 6'b0, econet_termen, econet_clken };

always @(posedge clk, posedge reset) begin
   if(reset) begin
      econet_clken <= 0;
      econet_termen <= 0;
      clkdiv <= 4'hF;
   end
   else if(wr && select) begin
      if(wr[0]) begin
         econet_clken <= data_in[0];
         econet_termen <= data_in[1];
      end
      if(wr[1]) begin
         clkdiv <= data_in[10:8];
      end
   end
end

reg [6:0]   clk_cntr = 0;
always @(posedge clk) begin
   clk_cntr <= clk_cntr + 1;
end

wire clk_cntr_out;
assign clk_cntr_out = clk_cntr[clkdiv];

reg [1:0]   econet_clk = 0;
always @(posedge clk_cntr_out)
   econet_clk <= econet_clk + 1;

// duty cycle 1/4
assign econet_clkout = econet_clk == 2'b11;

endmodule

