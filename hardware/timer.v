// One shot timer.
// Reset by setting bit 0 in REG_STATUS
module timer
(
   input          reset,

   input          clk,
   input [1:0]    addr,
   input [3:0]    wr,
   input          select,
   input [31:0]   data_in,
   output [31:0]  data_out,

   output         interrupt
);

parameter         REG_TIMERCNT      = 0;     // 0x00
parameter         REG_TIMERVAL      = 1;     // 0x04
parameter         REG_STATUS        = 2;     // 0x08

reg [31:0]        timerval;
reg [31:0]        count;
reg               enable;
reg               rst_timer;

`ifdef BENCH
   timerval = 0;
   count = 0;
   enable = 0;
   rst_timer = 0;
`endif

wire rst_counter = reset | rst_timer;
wire interrupt = count == timerval;

wire [31:0] data_out =
   addr == REG_TIMERCNT ?     count :
   addr == REG_TIMERVAL ?     timerval :
   addr == REG_STATUS   ?     { 30'b0, enable, interrupt } :
   32'h55555555; 

always @(posedge clk, posedge rst_counter) begin
   if(rst_counter)
      count <= 0;
   else if(enable && count < timerval) begin
      count <= count + 1;
   end
end

always @(posedge clk, posedge reset) begin
   if(reset) begin
      timerval <= 0;
      enable <= 0;
      rst_timer <= 0;
   end
   else if(wr && select) begin
      case(addr)
         REG_TIMERVAL: begin
            if(wr[0]) timerval[7:0]   <= data_in[7:0];
            if(wr[1]) timerval[15:8]  <= data_in[15:8];
            if(wr[2]) timerval[23:16] <= data_in[23:16];
            if(wr[3]) timerval[31:24] <= data_in[31:24];
         end

         REG_STATUS: begin
            if(wr[0]) begin
               rst_timer   <= data_in[0];
               enable      <= data_in[1];
            end
         end
      endcase
   end
   else rst_timer <= 0;
end

endmodule

