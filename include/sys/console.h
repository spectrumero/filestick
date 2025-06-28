#ifndef CONSOLE_H
#define CONSOLE_H
/*
;The MIT License
;
;Copyright (c) 2025 Dylan Smith
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

// Console ioctls

#define CONSOLE_SET_CONTROL   0x01000000

#define CONSOLE_RAW           1
#define CONSOLE_CLR_RAW       0xFFFE
#define CONSOLE_FLOW_XOFF     2
#define CONSOLE_CLR_

#define CONSOLE_SET_RAW       0x02000000
#define CONSOLE_SET_INTERACTIVE 0x03000000
#define CONSOLE_SET_FLOW_XOFF 0x04000000

#define FLOW_XOFF             0x13  // Ctrl-S
#define FLOW_XON              0x11  // Ctrl-Q

#endif
