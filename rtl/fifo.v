// FIFO (intended for the UART)
// The FIFO_SIZE parameter is best set to some multiple of the up5k block
// RAM sizes and certainly 2^something (e.g. 512, 1024).
// Each up5k block ram is 4kbit (512 bytes)
module fifo #(
   parameter FIFO_SIZE = 512
) (
   input    clk,
   input    reset,

   input    [7:0] data_in,
   output   [7:0] data_out,

   input    wr,
   input    rd,
   output   data_ready, 
   output   full,
   output   [15:0] bytes_avail
);
   // Register width for the FIFO pointers.
   parameter fifo_width = $clog2(FIFO_SIZE);

   reg [7:0]               fifo_buf[FIFO_SIZE];
   reg [7:0]               data_out;
   reg [fifo_width-1:0]    fifo_in_ptr;
   reg [fifo_width-1:0]    fifo_out_ptr;
   wire [fifo_width-1:0]   avail;

   assign data_ready = fifo_in_ptr != fifo_out_ptr;
   assign avail      = fifo_in_ptr - fifo_out_ptr;
   assign full       = (avail == FIFO_SIZE - 1);
   assign bytes_avail = avail;

   initial begin
      fifo_in_ptr <= 0;
      fifo_out_ptr <= 0;
   end

   // Read FIFO process
   always @(posedge clk) begin
      if(rd) begin
         data_out       <= fifo_buf[fifo_out_ptr];
         fifo_out_ptr   <= fifo_out_ptr + 1;
      end
   end

   // Write FIFO process
   always @(posedge clk) begin
      if(wr) begin
         fifo_buf[fifo_in_ptr]   <= data_in;
         fifo_in_ptr             <= fifo_in_ptr + 1;
      end
   end
endmodule


