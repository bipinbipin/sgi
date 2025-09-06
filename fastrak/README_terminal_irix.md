# 3SPACE Terminal for IRIX

This is an IRIX port of the original DOS terminal program for Polhemus 3SPACE/FASTRAK tracking instruments.

## Overview

The terminal program provides a complete interface to 3SPACE instruments including:
- Interactive terminal emulation
- Data logging capabilities  
- ASCII and binary data collection
- Continuous and point-mode operation
- Status reporting and configuration
- Batch command file processing

## Changes from DOS Version

This IRIX version replaces the following DOS-specific components:

- **Graphics**: DOS `graph.h` → curses library
- **Serial I/O**: MID driver → IRIX termios
- **Keyboard**: DOS BIOS → UNIX terminal I/O  
- **File I/O**: DOS-specific → POSIX file operations
- **Process Control**: DOS spawning → UNIX system calls

## Building

### Requirements
- IRIX 6.5 or compatible
- MIPSpro C compiler (`cc`)
- curses library (standard on IRIX)

### Compilation
```bash
make -f Makefile.terminal_irix
```

Or manually:
```bash
cc -O2 -fullwarn -n32 -o terminal_irix terminal_irix.c -lcurses -lm
```

## Usage

### Basic Usage
```bash
# Use default serial port (/dev/ttyd1)
./terminal_irix

# Specify serial port
./terminal_irix /dev/ttyd2
```

### Function Keys

| Key | Function |
|-----|----------|
| F1  | Continuous data transmission mode |
| F2  | Toggle ASCII/Binary output format |
| F3  | Enable/Disable data logging |
| F4  | Select log file |
| F5  | Display instrument status |
| F6  | Configure serial interface |
| F7  | Request single data record |
| F8  | Exit program |
| F9  | Display help |
| F10 | Batch mode (command files) |
| F11 | Toggle data display on/off |
| ENTER | Send command to instrument |

### Interactive Commands

Type 3SPACE commands directly and press ENTER to send:

- `P` - Request single data record
- `C` - Enter continuous mode  
- `c` - Exit continuous mode
- `F` - ASCII data format
- `f` - Binary data format
- `S` - Request status
- `R` - Reset to factory defaults

### Serial Port Configuration

Default serial settings:
- Baud rate: 9600
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

The program uses standard IRIX serial devices:
- `/dev/ttyd1` - Primary serial port (default)
- `/dev/ttyd2` - Secondary serial port
- `/dev/ttyf1` - First serial port on some systems

### Data Logging

The program can log all communication to/from the instrument:

1. Press F4 to select a log file
2. Choose default name (`sysdata.log`) or specify custom name
3. Press F3 to enable/disable logging
4. Data is logged in real-time during operation

### Batch Mode

Process command files containing multiple 3SPACE commands:

1. Press F10 to enter batch mode
2. Select option 0 to choose a command file
3. The file should contain one command per line
4. Use `<>` to represent carriage return in commands

Example batch file:
```
F
P
S
O1,2,3,4,5<>
C
```

## Serial Port Permissions

Ensure your user has access to the serial device:
```bash
# Check current permissions
ls -l /dev/ttyd1

# Add user to appropriate group (usually uucp)
# Or set permissions (as root)
chmod 666 /dev/ttyd1
```

## Troubleshooting

### Common Issues

**"Error opening serial port"**
- Check device exists: `ls -l /dev/ttyd*`
- Verify permissions
- Try different serial port

**"No Status Received"**  
- Check cable connections
- Verify instrument is powered on
- Check baud rate settings
- Try sending reset command (`R`)

**Function keys not working**
- Ensure terminal supports function keys
- Try using alternate key combinations
- Check terminal emulation settings

### Debug Information

The program provides error messages for:
- Serial port configuration failures
- File access problems  
- Communication timeouts
- Invalid responses from instrument

## Differences from Original

### Removed Features
- MID driver configuration menu (replaced with direct termios setup)
- DOS-specific video adapter detection
- Some DOS graphics formatting options

### Added Features  
- Command-line serial device selection
- Improved error handling
- UNIX-style file permissions handling
- Better timeout management

## Compatibility

This program is designed for IRIX but may work on other UNIX systems with minor modifications. The main dependencies are:

- POSIX serial I/O (termios)
- curses library
- Standard C library functions

## License

Copyright 1988-1994, Polhemus Incorporated, all rights reserved.
IRIX port modifications released under the same terms.

## See Also

- Original `TERMINAL.C` - DOS version
- `fastrak_reader.c` - Alternative IRIX Fastrak interface
- Polhemus instrument manuals for command reference
