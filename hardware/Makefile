# Adapted from the upduino Makefile.
NEXTPNR=nextpnr-ice40
YOSYS=yosys
ICEPACK=icepack
ICEPROG=iceprog
BINTOHEX=./bintohex
OBJS=bintohex.o

VERILOG_SRC=toplevel.v femtorv32_gracilis.v blkram.v uart_picosoc_shrunk.v MappedSPIFlash.v spicore.v ice40up5k_spram.v memmapper.v econet_rx_buffered.v econet_rx_phy.v econet_rx.v fcs.v

all:	bintohex toplevel.bin

bintohex:	$(OBJS)
	$(CC) -o $(BINTOHEX) $(OBJS)

toplevel.bin: toplevel.asc
	$(ICEPACK) toplevel.asc toplevel.bin

toplevel.asc: toplevel.json upduino.pcf
	$(NEXTPNR) --up5k --package sg48 --json toplevel.json --pcf upduino.pcf --asc toplevel.asc 

toplevel.json: $(BINTOHEX) $(VERILOG_SRC)
	$(BINTOHEX) ../build/boot/filestick-boot.bin filestick-boot.hex
	$(YOSYS) -q -p "synth_ice40 -json toplevel.json -top toplevel" $(VERILOG_SRC)

.PHONY: flash
flash:
	$(ICEPROG) -d i:0x0403:0x6014 toplevel.bin

.PHONY: clean
clean:
	$(RM) -f toplevel.json toplevel.asc toplevel.bin $(OBJS) $(BINTOHEX)
