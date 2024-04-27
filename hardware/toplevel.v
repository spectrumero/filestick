module toplevel (
   input wire     input_clk,

   input wire     econet_clkio,
   input wire     econet_rx,
   output wire    econet_tx,
   output wire    econet_tx_enable,
   output wire    econet_receiving, // debug
   output wire    econet_clken,
   output wire    term_en,
   output wire    collision_ref_pwm,
   input wire     collision_detect,
   
   output wire    led_red,
   output wire    led_green,
   output wire    led_blue,

   input wire     uart_rx,
   output wire    uart_tx,

   output [3:0]   spi_ss,
   output         flash_sck,     // spi_ss 0
   output         flash_mosi,
   input          flash_miso,
   output         spi_sck,       // spi_ss > 0
   output         spi_mosi,
   input          spi_miso,

   input          sd_present
);

// Configuration - add the general purpose timer
`define GP_TIMER
`define PLL_CLOCK

`ifdef PLL_CLOCK
   parameter CLOCK_HZ = 10000000;
   wire pll_out;
   wire pll_lock;
   wire clk;
   SB_PLL40_CORE #(
      .FEEDBACK_PATH("SIMPLE"),
      .DIVR(4'b0000),
      .DIVF(7'b0110100),
      .DIVQ(3'b101),
      .FILTER_RANGE(3'b001)
   ) pll1 (
      .LOCK(pll_lock),
      .RESETB(1'b1),
      .REFERENCECLK(input_clk),
      .PLLOUTGLOBAL(pll_out));
   reg clk_div;
   always @(posedge pll_out)
      clk_div <= clk_div + 1;
   SB_GB clkbuf(
      .USER_SIGNAL_TO_GLOBAL_BUFFER(clk_div),
      .GLOBAL_BUFFER_OUTPUT(clk));
`elsif DIVIDE_CLOCK
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

// FIXME: econet clock
wire econet_clkout;
wire econet_clk;

// econet clock I/O pin
// note that when output enabled, D_OUT_0 comes out
// on D_IN_0
SB_IO #(
   // PIN_OUTPUT_TRISTATE, PIN_INPUT
   .PIN_TYPE(6'b 1010_01)
   ) e_clkio (
      .PACKAGE_PIN(econet_clkio),
      .OUTPUT_ENABLE(econet_clken),
      .D_OUT_0(econet_clkout),
      .D_IN_0(econet_clk)
   );

// CPU bus
wire [31:0]    mem_addr;
wire [31:0]    mem_wdata;
wire [31:0]    mem_rdata;
wire           mem_rbusy;
wire           mem_wbusy;
wire [3:0]     cpu_we;
wire           cpu_rd;
wire           int;

wire  spram_sel      =  mem_addr[23:17] == 7'b0;   // 0x000000 - 0x01FFFF
wire  blkram_sel     =  mem_addr[23:17] == 7'b1;   // 0x020000
wire  rgbled_sel     =  mem_addr == 24'h800000;
`ifdef GP_TIMER
wire  timer_set_sel  =  mem_addr == 24'h800004;
wire  timer_ctl_sel  =  mem_addr == 24'h800008;
`endif
wire  uart_sel       =  mem_addr == 24'h80000C;
wire  uart_state_sel =  mem_addr == 24'h800010;
wire  sdstatus_sel   =  mem_addr == 24'h800014;
wire  spi_sel        =  mem_addr[23:4] == 20'h80002;

// Econet selectors
wire  econet_rx_buf_sel          = mem_addr[23:16] == 8'h81;
wire  econet_rx_reg_sel          = mem_addr[23:8]  == 16'h8001;
wire  econet_timer_a_sel         = mem_addr[23:4]  == 20'h80030;
wire  econet_hwctl_sel           = mem_addr        == 24'h800320;
wire  econet_tx_buf_sel          = mem_addr[23:16] == 8'h82;
wire  econet_tx_reg_sel          = mem_addr[23:8]  == 16'h8002;

// CPU memory read mux
assign mem_rdata =
   spram_sel         ? spram_rdata  :
   blkram_sel        ? blkram_rdata :
   spi_sel           ? spi_rdata    :
   uart_sel          ? uart_rdata   :
   uart_state_sel    ? uart_rstate  :
   `ifdef GP_TIMER
   timer_ctl_sel     ? { 31'b0, timer_intr } :
   `endif
   econet_rx_buf_sel ? econet_rx_data        :
   econet_rx_reg_sel ? econet_rx_data        :
   econet_tx_reg_sel ? econet_tx_reg_data    :
   econet_timer_a_sel ? econet_timer_a_data  :
   econet_hwctl_sel  ? econet_hwctl_data     :
   sdstatus_sel      ? sdstatus_rdata        :
   32'hDEADBEEF;

FemtoRV32 #(
   .RESET_ADDR(32'h20000)
   ) cpu (
   .clk(clk),
   .mem_addr(mem_addr),
   .mem_wdata(mem_wdata),
   .mem_wmask(cpu_we),
   .mem_rdata(mem_rdata),
   .mem_rstrb(cpu_rd),
   .mem_rbusy(mem_rbusy),
   .mem_wbusy(mem_wbusy),
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
wire econet_rx_valid;
wire econet_receiving;
buffered_econet econet_receiver(
   .reset(reset),
   .econet_clk(econet_clk),
   .econet_rx(econet_rx),
   .inhibit(econet_transmitting),

   .sys_clk(clk),
   .sys_rd(cpu_rd),
   .sys_wr(cpu_we),
   .sys_buf_select(econet_rx_buf_sel),
   .sys_reg_select(econet_rx_reg_sel),
   .sys_addr(mem_addr[11:2]),
   .sys_rdata(econet_rx_data),
   .sys_wdata(mem_wdata),
   .sys_frame_valid_out(econet_rx_valid),
   .receiving(econet_receiving));

wire econet_tx_data;
wire econet_tx_busy;
assign econet_tx = ~econet_tx_data;
wire econet_transmitting;
assign econet_tx_enable = econet_transmitting;
wire [31:0] econet_tx_reg_data;

econet_tx_buffered econet_transmitter(
   .reset(reset),
   .econet_clk(econet_clk),
   .econet_data(econet_tx_data),
   .transmitting(econet_transmitting),
   .receiving(econet_receiving),
   
   .sys_clk(clk),
   .sys_we(cpu_we),
   .sys_select(econet_tx_buf_sel),
   .sys_select_reg(econet_tx_reg_sel),
   .sys_addr(mem_addr[11:2]),
   .sys_wdata(mem_wdata),
   .sys_rdata(econet_tx_reg_data));

wire econet_timer_a_intr;
wire [31:0] econet_timer_a_data;
timer econet_timer_a(
   .reset(reset),
   .clk(clk),
   .wr(cpu_we),
   .select(econet_timer_a_sel),
   .addr(mem_addr[3:2]),
   .data_in(mem_wdata),
   .data_out(econet_timer_a_data),
   .interrupt(econet_timer_a_intr));

wire econet_collision_intr;
wire [31:0] econet_hwctl_data;
econet_hwctl ehctl(
   .reset(reset),
   .clk(clk),
   .wr(cpu_we),
   .select(econet_hwctl_sel),
   .data_in(mem_wdata),
   .data_out(econet_hwctl_data),
   .econet_clken(econet_clken),
   .econet_clkout(econet_clkout),
   .econet_termen(term_en));

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
`ifdef GP_TIMER
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
`endif // GP_TIMER

//---------------- uart -----------------
wire uart_wr_busy;
wire uart_valid;
wire uart_rd = uart_sel & cpu_rd;
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
   .BAUDS(230400)
   ) uart (
   .clk(clk),
   .resetq(!reset),

   .tx(uart_tx),
   .rx_raw(uart_rx),

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
wire [31:0]       spi_rdata;
spi #(
   .POLARITY(1)
   ) spicore (
      .reset(reset),
      .clk(clk),
      .we(cpu_we),
      .rd(cpu_rd),
      .select(spi_sel),
      .addr(mem_addr[3:2]),
      .wdata(mem_wdata),
      .wbusy(mem_wbusy),
      .rdata(spi_rdata),
      .rbusy(mem_rbusy),

      // flash has a dedicated SPI port
      // when spi_ss[0] is asserted
      .spi_ss(spi_ss),
      .spi_clk1(flash_sck),
      .spi_miso1(flash_miso),
      .spi_mosi1(flash_mosi),
      .spi_clk2(spi_sck),
      .spi_miso2(spi_miso),
      .spi_mosi2(spi_mosi));

// ---------- SD card detect ---------
// Note that data is handled by the SPI
// module. This just provides the interrupt
// when a card is inserted/removed and
// allows reading of the sd_present switch.
wire [31:0] sdstatus_rdata;
wire        sdcard_intr;
sdcard_detect sdcard (
   .reset(reset),
   .clk(clk),
   .select(sdstatus_sel),
   .we(cpu_we),
   .wdata(mem_wdata),
   .rdata(sdstatus_rdata),
   .pin_change(sdcard_intr),
   .sd_present(sd_present));

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

// ------- Collision ref PWM -----
// R=1k C=100nF 3dB pt = 1.6kHz
// TODO: actual PWM
reg [7:0]   pwm_ctr = 0;
always @(posedge clk) begin
   pwm_ctr <= pwm_ctr + 1;
end
assign collision_ref_pwm = pwm_ctr[7];

// ------- Interrupts ------------
`ifdef GP_TIMER
assign int = timer_intr | uart_valid | econet_rx_valid | econet_timer_a_intr | sdcard_intr;
`else
assign int = uart_valid | econet_rx_valid | econet_timer_a_intr | sdcard_intr;
`endif

endmodule

