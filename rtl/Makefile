# Adapted from the upduino Makefile.
NEXTPNR=nextpnr-ice40
YOSYS=yosys
ICEPACK=icepack
ICEPROG=iceprog
BINTOHEX=./bintohex
OBJS=bintohex.o

# upduino2.pcf or v1.pcf or v1-altserial.pcf
PCF?=v1.pcf

VERILOG_SRC=toplevel.v femtorv32_gracilis.v blkram.v fifo_uart.v fifo.v uart_picosoc_shrunk.v MappedSPIFlash.v spi.v ice40up5k_spram.v econet_rx_buffered.v econet_rx_phy.v econet_rx.v fcs.v econet_tx_buffered.v econet_tx_phy.v timer.v sdcard.v econet_hwctl.v

all:	bintohex toplevel.bin

bintohex:	$(OBJS)
	$(CC) -o $(BINTOHEX) $(OBJS)

toplevel.bin: toplevel.asc
	$(ICEPACK) toplevel.asc toplevel.bin

toplevel.asc: toplevel.json $(PCF)
	$(NEXTPNR) --freq 12 --timing-allow-fail --up5k --package sg48 --json toplevel.json --pcf $(PCF) --asc toplevel.asc 

toplevel.json: $(BINTOHEX) $(VERILOG_SRC)
	$(BINTOHEX) ../build/boot/filestick-boot.bin filestick-boot.hex
	$(YOSYS) -q -p "synth_ice40 -dsp -json toplevel.json -top toplevel" $(VERILOG_SRC)

.PHONY: flash
flash:
	$(ICEPROG) -d i:0x0403:0x6014 toplevel.bin

.PHONY: clean
clean:
	$(RM) -f toplevel.json toplevel.asc toplevel.bin $(OBJS) $(BINTOHEX)
