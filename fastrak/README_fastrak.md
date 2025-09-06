# Fastrak 3SP Reader for SGI IRIX

This program allows you to read position and orientation data from a Polhemus Fastrak 3SP motion tracking device connected to your SGI IRIX system via a serial port.

## Hardware Setup

1. **Connect the Fastrak 3SP to your SGI system:**
   - Use an RS-232 serial cable
   - Connect to one of the SGI serial ports (typically `/dev/ttyf1` or `/dev/ttyf2`)
   - Ensure the cable has proper pin assignments for RS-232 communication

2. **Power on the Fastrak 3SP device**

## Software Compilation

Compile the program using the provided Makefile:

```bash
make -f Makefile.fastrak
```

Or compile manually:

```bash
cc -O2 -fullwarn -o fastrak_reader fastrak_reader.c
```

## Usage

### Basic Usage

```bash
# Continuous reading from default port (/dev/ttyf2)
./fastrak_reader

# Continuous reading from specific port
./fastrak_reader -d /dev/ttyf1

# Single point reading
./fastrak_reader -d /dev/ttyf2 -p

# Show help
./fastrak_reader -h
```

### Command Line Options

- `-d <device>`: Specify serial device (default: `/dev/ttyf2`)
- `-p`: Point mode (single reading instead of continuous)
- `-h`: Show help

### Common SGI Serial Ports

- `/dev/ttyf1` - First serial port
- `/dev/ttyf2` - Second serial port  
- `/dev/ttyd1` - Dial-in port 1
- `/dev/ttyd2` - Dial-in port 2

## Communication Settings

The program automatically configures the serial port with these settings:
- **Baud Rate:** 19200
- **Data Bits:** 8
- **Parity:** Even (1 parity bit)
- **Stop Bits:** 1 (Note: 0 stop bits specified but using 1 as standard)
- **Flow Control:** None

## Output Format

The program displays data in this format:

```
Station | Position (X, Y, Z) | Orientation (Az, El, Ro)
--------|-------------------|------------------------
Station 1: Pos(-12.34, 5.67, -8.90) Orient(Az:123.4 El:-67.8 Ro:12.3)
```

Where:
- **Position**: X, Y, Z coordinates in inches
- **Orientation**: 
  - Az = Azimuth (rotation about Z-axis)
  - El = Elevation (rotation about Y-axis) 
  - Ro = Roll (rotation about X-axis)
  - All angles in degrees

## Fastrak 3SP Commands

The program sends these commands to initialize the device:
- `W\r` - Reset to defaults
- `A\r` - ASCII output format
- `O*\r` - Output list for all stations
- `C\r` - Continuous output mode (or `P\r` for point mode)

## Troubleshooting

### Device Not Found
- Check that the serial device exists: `ls -l /dev/ttyf*`
- Verify cable connections
- Try different serial ports

### No Data Received
- Ensure Fastrak 3SP is powered on
- Check cable wiring (may need null modem adapter)
- Verify the correct serial port is being used
- Try point mode first: `./fastrak_reader -p`

### Permission Denied
- Ensure you have read/write permissions to the serial device
- You may need to be root or member of appropriate group

### Garbled Data
- Check that no other process is using the serial port
- Verify cable quality and connections
- Ensure Fastrak is properly configured

## Testing

Run the test script to check your setup:

```bash
./test_fastrak.sh
```

This will help identify available serial ports and test basic connectivity.

## Technical Notes

- The program uses non-blocking I/O to avoid hanging
- Signal handlers (Ctrl+C) provide clean shutdown
- Buffer management prevents data overflow
- Compatible with SGI IRIX serial port drivers

## Fastrak 3SP Specifications

- **Range:** Up to 30 inches (76 cm) from transmitter
- **Resolution:** 0.03 inches (0.8 mm) position, 0.15° orientation
- **Update Rate:** Up to 120 Hz
- **Sensors:** Up to 4 receivers supported

For more detailed information about the Fastrak 3SP, consult the official Polhemus documentation.
