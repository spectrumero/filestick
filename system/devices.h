#ifndef DEVICES_H
#define DEVICES_H

// Device addresses and offsets
#define DEV_BASE           0x800000

#define OFFS_RGBLED        0
#define OFFS_TMRSET        4
#define OFFS_TMRCTL        8
#define OFFS_UART          12
#define OFFS_UARTSTATE     16
#define OFFS_SD_DETECT     0x14
#define OFFS_SPI_DAT       0x20
#define OFFS_SPI_IMM       0x24
#define OFFS_SPI_REG       0x28
#define OFFS_SPI_REG_BITCOUNT  0x28
#define OFFS_SPI_REG_SS    0x29
#define OFFS_SPI_REG_ENDIAN 0x2A
#define OFFS_SPI_REG_ACTIVE 0x2B

// Econet addresses and offsets
#define ECONET_RXBUF       0x810000
#define ECONET_TXBUF       0x820000

#define OFFS_RXSTART       0x100
#define OFFS_RXLEN         0x108
#define OFFS_REPLYADDR     0x110
#define OFFS_RXPORT        0x114
#define OFFS_RXFLAG        0x115
#define OFFS_RXSTATUS      0x11c
#define OFFS_MONITORMODE   0x11d

#define OFFS_TXSTART       0x200
#define OFFS_TXEND         0x204
#define OFFS_TXSTATUS      0x208

#define OFFS_TMR_A_SET     0x304
#define OFFS_TMR_A_STATUS  0x308

#define OFFS_NET_HWCTL     0x320

// Econet hardware bitfields
// Transmit
#define BIT_TX_TURNAROUND     1     // Indicates replying to another station
#define BIT_TX_BUSY           2     // Transmitter has a buffer to transmit
#define BIT_TX_TRANSMITTING   4     // Transmitter is actively transmitting

#define ECONET_MACHTYPE       0x00012F42  // version 1.0, machine 0x42 make 0x2F


#endif

