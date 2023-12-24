// This module only handles the SD card present/ejected
// interrupt generation. The actual SD card data is
// handled using the SPI module.
module sdcard_detect
(
   input          reset,
   input          clk,
   input          select,
   input [3:0]    we,

   output [31:0]  rdata,
   input  [31:0]  wdata,
   output         pin_change,

   input          sd_present_l         // active low
);

reg last_state;
reg pin_change;

// CPU can reset the state by writing 1 to the LSB.
wire rst_state = reset | (select & we[0] & wdata[0]);
wire sd_present = ~sd_present_l;
assign rdata = { 30'b0, sd_present, pin_change };

always @(posedge clk, posedge rst_state) begin
   if(rst_state) begin
      pin_change <= 0;
   end
   else begin
      if(sd_present != last_state && !pin_change) begin
         pin_change <= 1;
         last_state <= sd_present;
      end
   end
end

endmodule
