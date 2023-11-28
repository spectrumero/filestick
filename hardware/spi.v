module spi
#(
   parameter      POLARITY=0     // Negative edge
)
(
   // CPU interface
   input          reset,
   input          clk,
   input    [3:0] we,
   input          rd,
   input          select,
   input    [1:0] addr,          // CPU addr except bits [1:0]

   input    [31:0] wdata,
   output          wbusy,
   output   [31:0] rdata,
   output          rbusy,

   // SPI interface
   output         spi_clk,
   input          spi_miso,
   output         spi_mosi,
   output   [3:0] spi_ss
);

   parameter BYTE          =  0;
   parameter HALFWORD      =  1;
   parameter THREEBYTE     =  2;
   parameter WORD          =  3;

   parameter ADDR_DATAREG  =  0; // Set/read data reg and run a transfer
   parameter ADDR_IMMDATA  =  1; // Set/read data reg without running a transfer
   parameter ADDR_CTRLREG  =  2; // Set control register

   // Control registers
   reg [4:0]   reg_bitcount;     // initial bit count register (0..31)
   reg [1:0]   reg_ss;           // slave select
   reg         reg_big_endian;   // SPI words are big endian
   reg [31:0]  reg_write;        // data to write
   reg [31:0]  reg_read;         // data for cpu to read

   // SPI control
   reg [4:0]   bitcount;         // number of bits to shift
   reg [31:0]  shift_in;         // SPI input reg
   reg [31:0]  shift_out;        // SPI output reg
   reg         ss_active;        // slave select active

   // set output
   wire rd_datareg = (addr == ADDR_DATAREG) & select & rd;
   wire wr_datareg = (addr == ADDR_DATAREG) & select & (we != 0);
   wire wr_ctrlreg = (addr == ADDR_CTRLREG) & select & (we != 0);
   assign rdata = addr == ADDR_DATAREG ? reg_read :
                  addr == ADDR_IMMDATA ? reg_read :
                  addr == ADDR_CTRLREG ? { 7'b0, ss_active, 7'b0, reg_big_endian, 6'b0, reg_ss, 3'b0, reg_bitcount } :
                  32'hAAAAAAAA;

   // slave select output
   assign spi_ss = ss_active ? 
              (reg_ss == 0 ? 4'b1110 :
               reg_ss == 1 ? 4'b1101 :
               reg_ss == 2 ? 4'b1011 :
                             4'b0111)
               : 4'b1111;

   // Set control registers
   always @(posedge clk, posedge reset) begin
      if(reset) begin
         reg_bitcount <= 31;
         reg_big_endian <= 1;
         reg_write <= 0;
         reg_ss <= 0;
      end

      else if(wr_ctrlreg) begin
         if(we[0]) begin                        // sets number of bytes per trx
            case(wdata[1:0])
               BYTE:       reg_bitcount <= 7;
               HALFWORD:   reg_bitcount <= 15;
               THREEBYTE:  reg_bitcount <= 23;
               WORD:       reg_bitcount <= 31;
            endcase
         end

         if(we[1])      reg_ss <= wdata[9:8];         // sets which slave select is active
         if(we[2])      reg_big_endian <= wdata[16];  // sets endianness
      end
      else if(select && we && (addr == ADDR_DATAREG || addr == ADDR_IMMDATA)) begin
         if(reg_big_endian) begin
            if(we[0])      reg_write[7:0]    <= wdata[7:0];
            if(we[1])      reg_write[15:8]   <= wdata[15:8];
            if(we[2])      reg_write[23:16]  <= wdata[23:16];
            if(we[3])      reg_write[31:24]  <= wdata[31:24];
         end else begin
            if(we[0])      reg_write[31:24]  <= wdata[7:0];
            if(we[1])      reg_write[23:16]  <= wdata[15:8];
            if(we[2])      reg_write[15:8]   <= wdata[23:16];
            if(we[3])      reg_write[7:0]    <= wdata[31:24];
         end
      end
   end

   // wait control
   assign wbusy = (select && wrhold && addr == ADDR_DATAREG);

   reg rdhold;
   reg wrhold;
   assign rbusy = (select && rdhold && addr == ADDR_DATAREG && state == STATE_SHIFTING);

   // spi output
   assign spi_mosi = shift_out[31];
   assign spi_clk  = state == STATE_SHIFTING & (clk ^ POLARITY);

   // trx control
   wire trx_rq = rd_datareg | wr_datareg | wrhold;

   // SPI state machine
   parameter STATE_IDLE       = 2'b00;
   parameter STATE_SHIFTING   = 2'b01;
   parameter STATE_DONE       = 2'b10;
   reg [1:0] state;

   always @(posedge clk, posedge reset) begin
      if(reset) begin
         state <= STATE_IDLE;
         shift_out <= 0;
         rdhold <= 0;
         wrhold <= 0;
         reg_read  <= 0;
         ss_active <= 0;
      end
      else begin
         case(state)
            STATE_IDLE:
               if(trx_rq) begin

                  // CPU write request, so load SR immediately from wdata
                  if(wr_datareg) shift_out <= wdata;
                  else           shift_out <= reg_write;

                  state     <= STATE_SHIFTING;
                  bitcount  <= reg_bitcount;
                  ss_active <= 1;
                  if(rd_datareg) rdhold <= 1;
                  wrhold   <= 0;
               end
               else if(wr_ctrlreg & we[3]) ss_active <= wdata[24];

            STATE_SHIFTING: begin
               if(bitcount == 0) begin
                  reg_read <= shift_in;
                  state    <= STATE_IDLE;
                  rdhold   <= 0;
               end else begin
                  shift_out <= shift_out << 1;
                  bitcount <= bitcount - 1;
               end
               if(wr_datareg) wrhold <= 1;
            end
         endcase
      end
   end

   always @(negedge clk, posedge reset) begin
      if(reset)
         shift_in <= 0;
      else if(state == STATE_SHIFTING)
         shift_in <= (shift_in << 1) | spi_miso;
   end

endmodule

