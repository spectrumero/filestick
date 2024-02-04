# Econet NFS wire protocol

This describes the econet wire protocol for network filesystem operations.
Most of the information was derived from the MDFS manual chapter 10
and verifying the wire level protocol using wireshark between an emulated
BBC Master and the aund fileserver daemon, and checking the emulated
econet payloads.

Unless otherwise indicated, the data payloads all form part of a standard
4-way Econet handshake (scout, ack, data, data ack) sequence. This
document only concerns itself with the data payload.

## Ports

All fileserver commands are sent on port 0x90

Replies are expected to be on port 0x99

## Standard things with every data packet

A typical NetFS packet looks like this:

```
Bytes: 
0,1         Destination station and net
2,3         Source station and net
4           Reply port
5           Function code
6           User root dir context handle
7           Current directory context handle
8           Current library context handle
9...        Payload
```

The context handles are all set to 0x00 if a station hasn't yet
logged on, e.g. the first `*I AM` command will be sent with
bytes 6,7,8 set to 0x00.

## Typical NetFS logon sequence

Client station sends a machine peek to the fileserver:

```
FE 00       Dst: Station 254, net 0
19 00       Src: Station 25, net 0
88          Control byte - Machine peek
00          Port 0x00 (Immediate)
00 DB 00 00 Payload (ignored?)
```

Reply is as follows:
```
19 00       Dst: Station 25, net 0
FE 00       Src: Station 254, net 0
40 66 07 01 Payload: Type 0x40, Manufacturer 0x66, version 1.7
```

Then the `I AM` command is sent, with the handles all set to 0.
The data is sent to port 0x99 on the fileserver with the
normal 4-way Econet handshake. The data packet is as follows:

```
FE 00       Dst: Stn 254, net 0
19 00       Src: Stn 25, net 0
90          Reply port 0x90
00          Function code 0 (command line)
00          User root dir context handle set to 0
00          Current directory context handle set to 0
00          Current library context handle set to 0
49 20 41 4d 20 53 48 41 44 45 53 0d
            Payload (I AM SHADES\n)
```

In the case a password is supplied, the password is supplied
in plain text after the `I AM` command, e.g. if the password
is FOO BAR BAZ, the payload will be `I AM SHADES FOO BAR BAZ\n`.
Note that all this is in the clear, so don't use passwords
you care about for other things...

Curiously, if the command on the client is written as
`*I AM SHADES :` (so the password isn't echoed) the payload
is terminated by `0x0D 0x00` instead of just `0x0D`.

The fileserver replies on port 0x90 using the normal 4-way
Econet handshake. The data packet is as follows:

```
19 00       Dst: Station 25, net 0
FE 00       Src: Station 254, net 0
05          Command code 05, I AM
00          Result code
03          URD handle
05          CSD handle
06          LIB handle
00          Boot option (lowest 4 bits only)
```

The URD, CSD, LIB handles are then used with all subsequent
NetFS commands.

If the `I AM` command fails, the reply from the fileserver
will look like this:

```
19 00       Dst: Station 25, net 0
FE 00       Src: Station 254, net 0
00          Command code 00, No action, command complete
BB          FS error 0xBB, incorrect password
String      String "Incorrect password\n"
```

## Call 0 (Command line, including *I AM)

Payload is simply the command line terminated by a newline
character (0x0d) and without the `*`. For instance,
`*I AM SHADES` is sent as `49 20 41 4d 20 53 48 41 44 45 53 0d`

Example:
```
FE 00       Dst: Stn 254, net 0
19 00       Src: Stn 25, net 0
90          Reply port 0x90
00          Function code 0 (command line)
03          User root dir context handle
05          Current directory context handle
06          Current library context handle
....        Command line terminated by 0x0D
```

