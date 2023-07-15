module blkram (
   input          clk,
   input          select,
   input [3:0]    we,
   input          rd,
   input [11:0]   addr,
   input [31:0]   data_in,
   output [31:0]  data_out
);

reg [31:0]  mem[512];

reg [31:0]  d_reg;

assign data_out = d_reg;

initial begin
`ifdef BENCH
   $readmemh("../stage2/stage2.hex", mem);
`else
   $readmemh("filestick-boot.hex", mem);
`endif   
end

always @(posedge clk) begin
   if(we[3] && select) begin
      mem[addr][31:24] <= data_in[31:24];
   end
   if(we[2] && select) begin
      mem[addr][23:16] <= data_in[23:16];
   end
   if(we[1] && select) begin
      mem[addr][15:8] <= data_in[15:8];
   end
   if(we[0] && select) begin
      mem[addr][7:0] <= data_in[7:0];
   end

   if(rd && select) begin
      d_reg <= mem[addr];
   end
end   

endmodule


