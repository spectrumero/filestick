# File server function code summary (hex)

This was derived from the SJ Research MDFS manual, chapter 10, and converted
to hex to make it easier to look up function codes from a hex dump.

```
0x00        Command line
0x01        Save
0x02        Load
0x03        Examine
0x04        Catalogue header (Acorn only)
0x05        Load as command
0x06        Open file
0x07        Close file
0x08        Get byte
0x09        Put byte
0x0A  10    Get bytes
0x0B  11    Put bytes
0x0C  12    Read random access information
0x0D  13    Set random access information
0x0E  14    Read disc name information
0x0F  15    Read logged on users
0x10  16    Read date/time
0x11  17    Read EOF information
0x12  18    Read object information
0x13  19    Set object information
0x14  20    Delete object
0x15  21    Read user environment
0x16  22    Set user's boot option
0x17  23    Logoff
0x18  24    Read user information
0x19  25    Read file server version number
0x1A  26    Read file server free space
0x1B  27    Create directory, specifying size
0x1C  28    Set date/time
0x1D  29    Create file of specified size
0x1E  30    Read user free space (Acorn only)
0x1F  31    Set user free space (Acorn only)
0x20  32    Read client user identifier
0x40  64    Read account information (SJ Research only)
0x41  65    Read/write system information (SJ Research only)
```

