module toplevel (
   input wire     input_clk,

   input wire     econet_clk,
   input wire     econet_rx,
   output wire    econet_tx_p,
   output wire    econet_tx_n,
   output wire    econet_tx_enable,
   output wire    econet_receiving, // debug
   
   output wire    led_red,
   output wire    led_green,
   output wire    led_blue,

   input wire     uart_rx,
   output wire    uart_tx,

   output         spi_cs,
   output         spi_sck,
   output         flash_mosi,
   input          flash_miso
);

`ifdef DIVIDE_CLOCK
   parameter CLOCK_HZ = 6000000;
   wire clk;
   reg clk_div;
   always @(posedge input_clk)
      clk_div <= clk_div + 1;
   SB_GB clkbuf(
      .USER_SIGNAL_TO_GLOBAL_BUFFER(clk_div),
      .GLOBAL_BUFFER_OUTPUT(clk));
`else
   parameter CLOCK_HZ = 12000000;
   wire clk = input_clk;
`endif

// CPU bus
wire [31:0]    mem_addr;
wire [31:0]    mem_wdata;
wire [31:0]    mem_rdata;
wire [3:0]     cpu_we;
wire           cpu_rd;
wire           int;

wire  spram_sel      =  mem_addr[23:17] == 7'b0;   // 0x000000 - 0x01FFFF
wire  blkram_sel     =  mem_addr[23:17] == 7'b1;   // 0x020000
`ifdef USE_SLOWROM
wire  slowrom_sel    =  mem_addr[23:22] == 2'b01;  // 0x400000
`endif
wire  rgbled_sel     =  mem_addr == 24'h800000;
wire  timer_set_sel  =  mem_addr == 24'h800004;
wire  timer_ctl_sel  =  mem_addr == 24'h800008;
wire  uart_sel       =  mem_addr == 24'h80000C;
wire  uart_state_sel =  mem_addr == 24'h800010;
`ifndef USE_SLOWROM
wire  flashrom_spi_sel = mem_addr == 24'h800014;
wire  flashrom_spi_ss_rst = mem_addr == 24'h800018;
`endif
wire  econet_rx_buf_sel = mem_addr[23:16] == 8'h81;
wire  econet_rx_fs_sel  = mem_addr == 24'h800030;
wire  econet_rx_fe_sel  = mem_addr == 24'h800034;
wire  econet_tx_buf_sel = mem_addr[23:16] == 8'h82;
wire  econet_tx_sel_frame_start = mem_addr == 24'h800040;
wire  econet_tx_sel_frame_end   = mem_addr == 24'h800044;
wire  econet_tx_state_sel = mem_addr == 24'h800048;

// CPU memory read mux
assign mem_rdata =
   spram_sel         ? spram_rdata  :
   blkram_sel        ? blkram_rdata :
   `ifdef USE_SLOWROM
   slowrom_sel       ? slowrom_rdata :
   `else
   flashrom_spi_sel  ? { 23'b0, flashrom_spi_ready, flashrom_spi_rdata } :
   `endif
   uart_sel          ? uart_rdata   :
   uart_state_sel    ? uart_rstate  :
   timer_ctl_sel     ? { 31'b0, timer_intr } :
   econet_rx_buf_sel ? econet_rx_data        :
   econet_rx_fs_sel  ? econet_rx_frame_start :
   econet_rx_fe_sel  ? econet_rx_frame_end   :
   econet_tx_state_sel ? {30'b0, econet_transmitting, econet_tx_busy } :
   32'hAAAAAAAA;

FemtoRV32 #(
   .RESET_ADDR(32'h20000)
   ) cpu (
   .clk(clk),
   .mem_addr(mem_addr),
   .mem_wdata(mem_wdata),
   .mem_wmask(cpu_we),
   .mem_rdata(mem_rdata),
   .mem_rstrb(cpu_rd),
   .mem_rbusy(slowrom_rbusy),
   .mem_wbusy(1'b0),
   .interrupt_request(int),
   .reset(~reset)
);

wire [31:0] blkram_rdata;
blkram dpram (
   .clk(clk),
   .select(blkram_sel),
   .we(cpu_we),
   .rd(cpu_rd),
   .addr(mem_addr[13:2]),
   .data_in(mem_wdata),
   .data_out(blkram_rdata)
);

wire [31:0] spram_rdata;
ice40up5k_spram spram (
   .clk(clk),
   .select(spram_sel),
   .wen(cpu_we),
   .addr(mem_addr[16:2]),
   .wdata(mem_wdata),
   .rdata(spram_rdata)
);

wire [31:0] econet_rx_data;
wire [31:0] econet_rx_frame_start;
wire [31:0] econet_rx_frame_end;
wire econet_rx_valid;
wire econet_receiving;
buffered_econet econet_receiver(
   .reset(reset),
   .econet_clk(econet_clk),
   .econet_rx(econet_rx),
   .inhibit(econet_transmitting),
   .sys_clk(clk),
   .sys_rd(cpu_rd),
   .sys_select(econet_rx_buf_sel),
   .sys_addr(mem_addr[9:2]),
   .sys_data(econet_rx_data),
   .sys_frame_start(econet_rx_frame_start),
   .sys_frame_end(econet_rx_frame_end),
   .sys_frame_valid(econet_rx_valid),
   .receiving(econet_receiving));

wire econet_tx_data;
wire econet_tx_busy;
assign econet_tx_p = econet_tx_data;
assign econet_tx_n = ~econet_tx_data;
wire econet_transmitting;
assign econet_tx_enable = ~econet_transmitting;

econet_tx_buffered econet_transmitter(
   .reset(reset),
   .econet_clk(econet_clk),
   .econet_data(econet_tx_data),
   .transmitting(econet_transmitting),
   .busy(econet_tx_busy),
   .receiving(econet_receiving),
   
   .sys_clk(clk),
   .sys_we(cpu_we),
   .sys_select(econet_tx_buf_sel),
   .sys_addr(mem_addr[9:2]),
   .sys_data(mem_wdata),
   .sys_select_frame_start(econet_tx_sel_frame_start),
   .sys_select_buffer_end(econet_tx_sel_frame_end));


`ifdef USE_SLOWROM
wire [31:0] slowrom_rdata;
wire        slowrom_rbusy;
wire        slowrom_rstrb = slowrom_sel & cpu_rd;
`define UPDUINO 1
MappedSPIFlash slowrom (
   .clk(clk),
   .rstrb(slowrom_rstrb),
   .word_address(mem_addr[21:2]),
   .rdata(slowrom_rdata),
   .rbusy(slowrom_rbusy),

   .CLK(spi_sck),
   .CS_N(spi_cs),
   .MOSI(flash_mosi),
   .MISO(flash_miso)
);
`else
wire     slowrom_rbusy = 0;
`endif

// -------   Devices ------
// RGB LED
reg [2:0] rgb_led;
always @(posedge clk) begin
   if(cpu_we[0] && rgbled_sel)
      rgb_led <= mem_wdata[2:0];
end   

SB_RGBA_DRV RGB_DRIVER (
   .RGBLEDEN(1'b1),
   .RGB0PWM(rgb_led[1]),
   .RGB1PWM(rgb_led[0]),
   .RGB2PWM(rgb_led[2]),
   .CURREN (1'b1),
   .RGB0(led_green),
   .RGB1(led_blue),
   .RGB2(led_red));
  defparam RGB_DRIVER.RGB0_CURRENT = "0b000001";
  defparam RGB_DRIVER.RGB1_CURRENT = "0b000001";
  defparam RGB_DRIVER.RGB2_CURRENT = "0b000001";

// ----- Timer -------
reg [31:0]  timer_reg;
reg [31:0]  timer_stop;
reg         timer_enable;
reg         timer_intr;

wire timer_ctl_wr    = timer_ctl_sel && cpu_we != 0;
wire timer_done      = timer_reg == timer_stop;
wire timer_rst       = (timer_ctl_wr & mem_wdata[0]) | reset;
wire timer_int_ack   = timer_ctl_wr & mem_wdata[1];

always @(posedge clk, posedge reset) begin
   if(reset) begin
      timer_stop <= 32'hFFFFFFFF;
   end
   else if(timer_set_sel) begin
      if(cpu_we[3])
         timer_stop[31:24] <= mem_wdata[31:24];
      if(cpu_we[2])
         timer_stop[23:16] <= mem_wdata[23:16];
      if(cpu_we[1])
         timer_stop[15:8] <= mem_wdata[15:8];
      if(cpu_we[0])
         timer_stop[7:0] <= mem_wdata[7:0];
   end
end   

always @(posedge clk, posedge reset)
   if(reset)
      timer_enable <= 0;
   else
      if(timer_ctl_wr)
         if(mem_wdata[2])
            timer_enable <= 1;
         else
            timer_enable <= 0;

always @(posedge clk, posedge timer_rst)
   if(timer_rst)
      timer_reg <= 0;
   else if(timer_enable)
      if(timer_reg > timer_stop)
         timer_reg <= 0;
      else
         timer_reg <= timer_reg + 1;

wire timer_intr_reset = reset | timer_int_ack;
always @(posedge clk, posedge timer_intr_reset)
   if(timer_intr_reset)
      timer_intr <= 0;
   else if(timer_done)
      timer_intr <= 1;

//---------------- uart -----------------
wire uart_wr_busy;
wire uart_valid;
wire uart_rd = uart_state_sel & cpu_rd;
wire uart_wr = uart_sel & cpu_we[0];
wire [7:0] uart_rx_data;

parameter WR_UART_IDLE=0;
parameter WR_UART_WRITING=1;
parameter WR_UART_DONE_WRITING=2;
reg [1:0] uart_wr_busy_state;

wire [31:0] uart_rdata = { 20'b0, uart_wr_busy_state, uart_wr_busy, uart_valid, uart_rx_data };
wire [31:0] uart_rstate = { 28'b0, uart_wr_busy_state, uart_wr_busy, uart_valid };

buart #(
   .FREQ_HZ(CLOCK_HZ),
   .BAUDS(115200)
   ) uart (
   .clk(clk),
   .resetq(!reset),

   .tx(uart_tx),
   .rx(uart_rx),

   .wr(uart_wr),
   .rd(uart_rd),

   .tx_data(mem_wdata[7:0]),
   .rx_data(uart_rx_data),

   .busy(uart_wr_busy),
   .valid(uart_valid));

always @(posedge clk)
   if(reset | uart_rd)
      uart_wr_busy_state <= 0;
   else
      case(uart_wr_busy_state) 
         WR_UART_IDLE:
            if(uart_wr_busy) uart_wr_busy_state <= WR_UART_WRITING;
         WR_UART_WRITING:
            if(!uart_wr_busy) uart_wr_busy_state <= WR_UART_DONE_WRITING;
      endcase
         

//-------end-uart-------------

//-------spi------------------
`ifndef USE_SLOWROM
wire flashrom_spi_we = flashrom_spi_sel & cpu_we[0];
wire [7:0] flashrom_spi_rdata;
wire       flashrom_spi_ready;
spicore #(
   .POLARITY(1)
   ) flashrom_spi (
   .reset(reset),
   .clk(clk),
   .we(flashrom_spi_we),
   .spi_di(mem_wdata[7:0]),
   .spi_do(flashrom_spi_rdata),
   .ready(flashrom_spi_ready),
   .spi_ss_reset(flashrom_spi_ss_rst),

   .spi_clk(spi_sck),
   .spi_ss(spi_cs),
   .spi_miso(flash_miso),
   .spi_mosi(flash_mosi));
`endif

// ---------- Reset -----------
reg         reset = 1;
reg [3:0]   reset_cnt = 0;
always @(posedge clk)
   if(reset_cnt == 4'hF)
      reset <= 0;
   else begin
      reset <= 1;
      reset_cnt <= reset_cnt + 1;
   end

// ------- Interrupts ------------
assign int = timer_intr | uart_valid; // | econet_rx_valid;

endmodule

