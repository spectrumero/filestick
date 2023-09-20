#ifndef DEVICES_H
#define DEVICES_H

// Device addresses and offsets
#define DEV_BASE           0x800000

#define OFFS_RGBLED        0
#define OFFS_TMRSET        4
#define OFFS_TMRCTL        8
#define OFFS_UART          12
#define OFFS_UARTSTATE     16
#define OFFS_SPIFLASH      20
#define OFFS_SPI_SS        24

// Econet addresses and offsets
#define ECONET_RXBUF       0x810000
#define ECONET_TXBUF       0x820000

#define OFFS_RXSTART       0x100
#define OFFS_RXLEN         0x108
#define OFFS_REPLYADDR     0x110
#define OFFS_RXPORT        0x114
#define OFFS_RXSTATUS      0x11c

#define OFFS_TXSTART       0x200
#define OFFS_TXEND         0x204
#define OFFS_TXSTATUS      0x208

#define OFFS_TMR_A_SET     0x304
#define OFFS_TMR_A_STATUS  0x308

// Econet hardware bitfields
// Transmit
#define BIT_TX_TURNAROUND     1     // Indicates replying to another station
#define BIT_TX_BUSY           2     // Transmitter has a buffer to transmit
#define BIT_TX_TRANSMITTING   4     // Transmitter is actively transmitting

#endif

