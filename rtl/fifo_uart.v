// FIFO'd UART

module fifo_uart #(
   parameter FREQ_HZ = 10000000,
   parameter BAUDS   = 115200
) (
   input    clk,
   input    reset,

   output   tx,
   input    rx,

   input    wr,
   input    rd,

   input  [7:0] tx_data,
   output [7:0] rx_data,

   output busy,
   output data_ready,
   output [15:0] bytes_avail,
   output cts
);

wire [7:0]  uart_rx_data;
wire        uart_rx_valid;
reg         uart_rx_fifo_write;
reg         uart_read_state;
reg         uart_rd;

// Receive a byte into the FIFO
always @(posedge clk, posedge reset) begin
   if(reset) begin
      uart_rx_fifo_write <= 0;
      uart_rd <= 0;
      uart_read_state <= 0;
   end

   else begin
      case(uart_read_state)
         0: begin
            if(uart_rx_valid && !cts) begin
               uart_rx_fifo_write <= 1;
               uart_rd <= 1;
               uart_read_state <= 1;
            end
         end
         1: begin
            uart_rx_fifo_write <= 0;
            uart_rd <= 0;
            uart_read_state <= 0;
         end
      endcase
   end
end

// The FIFO
fifo uart_rx_fifo (
   .clk(clk),
   .reset(reset),

   .data_out(rx_data),
   .data_in(uart_rx_data),

   .rd(rd),
   .wr(uart_rx_fifo_write),
   .data_ready(data_ready),
   .full(cts),
   .bytes_avail(bytes_avail)
);

// The UART
buart #(
   .FREQ_HZ(FREQ_HZ),
   .BAUDS(BAUDS)
) uart (
   .clk(clk),
   .resetq(!reset),

   .tx(tx),
   .rx_raw(rx),

   .wr(wr),
   .rd(uart_rd),

   .tx_data(tx_data),
   .rx_data(uart_rx_data),

   .busy(busy),
   .valid(uart_rx_valid)
);

endmodule
