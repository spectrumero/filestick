module spicore
   #(
      parameter POLARITY=0 // negative edge
   )
   (
      input reset,
      input clk,
      input we,            // write enable loads a byte and starts things
      input spi_ss_reset,
      input [7:0] spi_di,
      output [7:0] spi_do,
      output ready,
   
      output spi_clk,
      input spi_miso,
      output spi_mosi,
      output spi_ss
   );

   parameter STATE_IDLE = 2'b00;
   parameter STATE_SHIFTING = 2'b01;
   parameter STATE_RSTFLASH = 2'b10;
   parameter STATE_RSTSEND  = 2'b11;

   reg [7:0] shift_in;
   reg [7:0] shift_out;
   reg [7:0] spi_do;
   reg [1:0] state;
   reg [4:0] bitcount;
   reg ready;
   reg spi_ss;

   assign spi_mosi = shift_out[7];
   assign spi_clk = ((state == STATE_SHIFTING || state == STATE_RSTSEND)) & (clk ^ POLARITY);

   always @(posedge clk, posedge reset) begin
      if(reset) begin
         shift_out <= 0;
         ready <= 0;
         state <= STATE_RSTFLASH;
         spi_ss <= 1;
      end
      else if(spi_ss_reset)
         spi_ss <= 1;
      else
         case(state)
            STATE_IDLE:
               if(we) begin
                  ready <= 0;
                  bitcount <= 0;
                  shift_out <= spi_di;
                  state <= STATE_SHIFTING;
                  spi_ss <= 0;
               end
            STATE_SHIFTING:
               if(bitcount == 7) begin
                  spi_do <= shift_in;
                  ready <= 1;
                  state <= STATE_IDLE;
               end
               else begin
                  shift_out <= shift_out << 1;
                  bitcount <= bitcount + 1;
               end
            STATE_RSTFLASH:
            begin
               bitcount <= 0;
               state <= STATE_RSTSEND;
               shift_out[7] <= 1;
            end
            STATE_RSTSEND:
               if(bitcount == 16) begin
                  spi_ss <= 1;
                  ready  <= 1;
                  state  <= STATE_IDLE;
               end
               else begin
                  spi_ss <= 0;
                  bitcount <= bitcount + 1;
               end

         endcase
   end

   // sample input halfway through the cycle
   always @(negedge clk, posedge reset) begin
      if(reset)
         shift_in <= 0;
      else if(state == STATE_SHIFTING)
         shift_in <= (shift_in << 1) | spi_miso;
   end

endmodule   
