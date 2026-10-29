# BBC BASIC for the FileStick

A BBC BASIC II interpreter written in C for the FileStick. It builds to
about 28K of code for rv32imc, leaving about 33K free for BASIC programs.

```
*> basic.elf
BBC BASIC for FileStick

33786 bytes free

>
```

`basic.elf prog` starts BASIC and CHAINs `prog`. `*BYE` (or `*QUIT`)
returns to init.

## Building

It's built with the rest of the software (`cmake -B build; cd build; make`),
giving `build/basic/basic.elf`. Copy it to the flash or SD filesystem.

For testing, a host build runs on Linux (x86-64), using stdin and stdout
as the console:

```
$ gcc -O2 -o basic main.c eval.c stmt.c token.c num.c io.c
```

## Compatibility

Programs are stored in memory in BBC BASIC II's tokenised format. A program
SAVEd on a BBC Micro can be LOADed and run, and the other way round. LOAD
also accepts plain text files, so programs can be written on another
computer. Lines without line numbers are numbered in steps of 10.

Numbers work as they do in BBC BASIC II. Integers are 32 bit. Reals are
40 bit (32 bit mantissa), with the same range and precision as on the BBC
Micro, and `@%` print formatting works the same way. PRINT# and INPUT# use
the BBC Micro's data file format.

The keyword table, abbreviations (`P.` for PRINT etc.), error numbers and
messages all come from BASIC II.

Everything in BASIC II is implemented except for:

* **The 6502 assembler.** CALL and USR call RISC-V machine code instead
  (see below).
* **Graphics and sound.** MODE clears the screen, COLOUR sets ANSI terminal
  colours, and CLG, GCOL, MOVE, DRAW, PLOT, SOUND and ENVELOPE evaluate
  their arguments and do nothing else. POINT and ADVAL return 0.
* **INKEY with a negative number** (scanning for a particular key) always
  returns FALSE.

## The console

The console is a serial terminal, so VDU codes are translated to ANSI
escape sequences where there is an equivalent: 12 (CLS) and 22 (MODE)
clear the screen, 30 homes the cursor, 31 (and `PRINT TAB(x,y)`) moves it,
17 (COLOUR) sets the colour, and 8, 9, 10, 11 and 13 move the cursor. VDU
27 is passed through, so ANSI sequences can be sent with VDU or CHR$27.
The parameters of other multi-byte VDU codes are discarded.

Escape stops a program, as on the BBC Micro. A program's keyboard input
and typeahead go through a 32 character buffer.

Lines are edited with backspace/delete, and Ctrl-U deletes the whole line.

## Memory

BASIC gets its workspace from the system's malloc when it starts. PAGE,
TOP, LOMEM and HIMEM are real addresses in this block, so `?`, `!` and `$`
work as expected on memory reserved with `DIM`, and on any other address
above &10000 (including the devices at &800000; `!` on an aligned
address does a single word access).

The C stack lives below HIMEM, so the space for PROC/FN calls, LOCAL
variables and expression evaluation comes out of the same free memory as
variables. Roughly 170 levels of PROC recursion, or 70 levels of FN
recursion within expressions, fit in an empty workspace. HIMEM can be
lowered to reserve memory, but not inside a PROC or FN.

## CALL and USR

`CALL addr` and `USR(addr)` call RISC-V code with A%, X%, Y% and C% in
a0-a3. USR returns a0. For example, an add function:

```
DIM code% 8
code%!0 = &00B50533 : REM add a0, a0, a1
code%!4 = &00008067 : REM ret
A% = 40 : X% = 2 : PRINT USR(code%)
```

## Files and * commands

OPENIN, OPENOUT, OPENUP, BGET#, BPUT#, PRINT#, INPUT#, PTR#, EXT#, EOF#
and CLOSE# work on files on any mounted filesystem. Channel numbers are
file descriptors. `CLOSE#0` closes everything.

The * commands (also available with OSCLI) are:

| Command | |
|---|---|
| `*CAT [dir]` or `*. [dir]` | list a directory |
| `*CD dir` or `*DIR dir` | change directory |
| `*DELETE file` | delete a file |
| `*MKDIR dir` | make a directory |
| `*BYE` or `*QUIT` | leave BASIC |
| `*FX ...` | ignored |

## Source files

| File | |
|---|---|
| `main.c` | startup, errors, the command line, program editing, LOAD and SAVE |
| `token.c` | the keyword table, tokenising and LIST |
| `eval.c` | variables, arrays and expression evaluation |
| `stmt.c` | statements, PROC/FN calls and the execution loop |
| `num.c` | floating point, maths functions, number conversion |
| `io.c` | console, VDU, keyboard, files and * commands |
| `stack.S` | runs BASIC on the stack at HIMEM |
