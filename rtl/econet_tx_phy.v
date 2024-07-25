module econet_tx
(
   input       reset,
   input       econet_clk,
   input [7:0] tx_byte,
   input       start_frame,
   input       end_frame,
   output      request_byte,
   output      transmitting,

   output      econet_data
);

parameter      STATE_IDLE        = 0;
parameter      STATE_START_SYNC  = 1;
parameter      STATE_TRANSMIT    = 2;
parameter      STATE_END_SYNC    = 3;

reg [1:0]      state;
reg [7:0]      txbuf;
reg [2:0]      bitcount;
reg [2:0]      onecount;
reg            data_out;

// invert output
assign         econet_data = ~data_out;

`ifdef BENCH
   initial begin
      state = 0;
      bitcount = 0;
      data_out = 1;
      onecount = 0;
      transmitting = 0;
   end
`endif

always @(negedge econet_clk, posedge reset) begin
   if(reset)
      txbuf <= 0;
   else if(request_byte)
      txbuf <= tx_byte;
end

wire request_byte = (bitcount == 7 && state != STATE_END_SYNC);
wire output_bit   = (txbuf >> bitcount) & 1;

always @(negedge econet_clk, posedge reset) begin
   if(reset) begin
      state <= STATE_IDLE;
      data_out <= 1;
      onecount <= 0;
   end
   else
      case(state)

         STATE_IDLE: begin
            if(start_frame) state <= STATE_START_SYNC;
            bitcount <= 0;
            data_out <= 1;
         end

         STATE_START_SYNC: begin
            if(bitcount == 0)       data_out <= 0;
            else if(bitcount < 7)   data_out <= 1;
            else begin
               data_out <= 0;
               state <= STATE_TRANSMIT;
            end
            bitcount <= bitcount + 1;
         end

         STATE_TRANSMIT: begin
            if(output_bit == 0) onecount <= 0;
            else                onecount <= onecount + 1;

            if(onecount == 5) begin
               onecount <= 0;
               data_out <= 0;    // insert a zero
            end
            else begin
               if(bitcount == 7 && end_frame) state <= STATE_END_SYNC; 

               data_out <= output_bit;
               bitcount <= bitcount + 1;
            end
         end

         STATE_END_SYNC: begin
            if(bitcount == 0)       data_out <= 0;
            else if(bitcount < 7)   data_out <= 1;
            else begin
               data_out <= 0;
               state <= STATE_IDLE;
            end
            bitcount <= bitcount + 1;
         end
            
      endcase
   end

   reg transmitting;
   always @(negedge econet_clk)
      transmitting <= state != STATE_IDLE;

endmodule

