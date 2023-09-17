# FileStick device addresses

.set  device_base,      0x800000
.set  offset_rgbled,    0
.set  offset_timerset,  4
.set  offset_timerctl,  8
.set  offset_uart,      12
.set  offset_uartstate, 16
.set  offset_spiflash,  20
.set  offset_spi_ss,    24

# Other definitions
.set  timer_ten_ms,     120000
.set  timer_hundred_ms, 1200000
.set  timer_quarter_sec,3000000
.set  timer_one_sec,    12000000

.set  timer_enable,     0x02
.set  timer_reset,      0x01

