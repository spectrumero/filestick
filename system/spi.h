#ifndef SPI_H
#define SPI_H
/*
;The MIT License
;
;Copyright (c) 2023 Dylan Smith
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;THE SOFTWARE.
*/

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// Deassert slave select.
void spi_deassert_ss();

// Select slave.
// Valid values are 0 to 3.
void spi_set_slave(uint8_t slave);

// Write data to SPI
void spi_write(void *src, size_t size, bool deassert_ss_when_done);

// Read data from SPI
// wr_word is the data to write while reading, normally should be set to
// a value such as 0xFFFFFFFF or 0x0 depending on the device.
void spi_read(void *dst, size_t size, bool deassert_ss_when_done, uint32_t wr_word);

// Write/read a single byte. Writes wr_byte and returns what was transferred back.
uint8_t spi_byte(uint8_t wr_byte);

#endif
