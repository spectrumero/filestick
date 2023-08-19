// 
module econet_rx
(
   input reset,
   input econet_clk,
   input econet_data,

   output [1:0] phy_state,
   output [7:0] data_out,
   output       data_strobe,
   output       frame_start,
   output       frame_end
);

// ADLC start/end sync pattern
parameter SYNC_PATTERN = 8'b01111110;
parameter SYNC_ERROR   = 8'b01111111;

// Physical link state machine
parameter STATE_PHY_IDLE = 0;       // Line is idling
parameter STATE_PHY_RX_SYNC = 1;    // Zero detected, test for sync pattern
parameter STATE_PHY_SYNC = 2;       // Sync successfully received
parameter STATE_PHY_WAIT_IDLE = 3;  // Wait for line to go idle

reg[7:0] rxbuf;
reg[7:0] data_out;
reg[1:0] phy_state;
reg[3:0] one_counter;
reg[3:0] bit_counter;
reg      data_strobe;
reg      end_sync;
reg      frame_start;         // tell stuff outside a sync has been received
reg      frame_end;           // and ended

// Econet clock: transitions are on the negedge,
// sampling is done on the posedge.
always @(posedge econet_clk, posedge reset) begin
   if(reset) begin
      rxbuf <= 0;
      phy_state <= STATE_PHY_IDLE;
      one_counter <= 0;
      data_out <= 0;
      bit_counter <= 0;
      data_strobe <= 0;
      end_sync <= 0;
      frame_start <= 0;
      frame_end <= 0;
   end

   else begin
      case (phy_state) 

         STATE_PHY_IDLE: begin
            one_counter <= 0;
            end_sync <= 0;
            frame_start <= 0;
            frame_end <= 0;

            // econet idles at 1, so the moment we detect a zero we know
            // someone is transmitting.
            if(econet_data == 1'b0) begin
               phy_state <= STATE_PHY_RX_SYNC;
            end
         end

         STATE_PHY_RX_SYNC: begin
            if(econet_data == 1'b1) begin

               if(one_counter == 6)
                  // too many 1s, error of some kind but the line is probably
                  // idle, wait for a new sync.
                  phy_state <= STATE_PHY_IDLE;
               one_counter <= one_counter + 1;

            end
            // zero received
            else begin
               // sync pattern is 01111110 so anything other than 6 1s
               // when a 0 arrives is an error/abort of some kind or the line
               // went idle
               if(one_counter != 6) begin
                  phy_state <= STATE_PHY_WAIT_IDLE;
                  one_counter <= 0;
               end
               else begin
                  // Sync received, the data potentially is about to start or
                  // a frame ended
                  if(end_sync) begin   // frame ended
                     frame_end <= 1;
                     phy_state <= STATE_PHY_IDLE;
                  end
                  else begin
                     // frame maybe starting
                     frame_start <= 1;
                     phy_state <= STATE_PHY_SYNC;
                  end

                  one_counter <= 0;
                  bit_counter <= 0;
                  rxbuf <= 0;
                  data_out <= 0;
               end
            end
         end

         STATE_PHY_SYNC: begin
            frame_start <= 0;
            if(econet_data == 1'b1) begin
               // if we already have 5 1s, this is probably another sync pattern.
               if(one_counter == 5)
                  phy_state <= STATE_PHY_RX_SYNC;

               one_counter <= one_counter + 1;

               if(bit_counter == 7) begin
                  data_out <= rxbuf[7:1] | 8'h80;
                  bit_counter <= 0;
                  data_strobe <= 1;
                  end_sync <= 1;
               end
               else begin
                  data_strobe <= 0;
                  rxbuf <= 8'h80 | (rxbuf >> 1);
                  bit_counter <= bit_counter + 1;
               end
            end
            else begin
               // is this zero breaking up a run of 1s?
               // in which case we can just ignore it
               if (one_counter == 5) begin
                  one_counter <= 0;
               end
               else begin
                  one_counter <= 0;

                  if(bit_counter == 7) begin
                     data_out <= rxbuf[7:1];
                     bit_counter <= 0;
                     data_strobe <= 1;
                     end_sync <= 1;
                  end
                  else begin
                     data_strobe <= 0;
                     rxbuf <= rxbuf >> 1;
                     bit_counter <= bit_counter + 1;
                  end
               end
            end
         end
         

         // wait for at least 7 1s
         STATE_PHY_WAIT_IDLE: begin
            if(econet_data == 1'b1) begin
               if(one_counter == 7)
                  phy_state <= STATE_PHY_IDLE;
               one_counter <= one_counter + 1;
            end
            else // 0 received, possible new sync? 
               phy_state <= STATE_PHY_RX_SYNC;

         end
      endcase
   end // if
end // always

endmodule
