//
module fcs
(
   input reset,
   input econet_clk,
   input [7:0] data,
   input enable,

   output [15:0] fcsval
);

parameter FCSINIT = 16'hFFFF;
parameter FCSGOOD = 16'hF0B8;

reg [15:0] fcstab[256];
reg [15:0] fcsval;

initial begin
   $readmemh("fcstab.hex", fcstab);
end

always @(posedge econet_clk, posedge reset) begin
   if(reset) begin
      fcsval <= FCSINIT;
   end
   else if(enable) begin
      fcsval <= (fcsval >> 8) ^ fcstab[(fcsval[7:0] ^ data)];
   end
end


endmodule

