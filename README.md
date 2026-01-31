# PassFlow System

A multi-threaded C++ application for Raspberry Pi 5 that manages camera recording and peripheral device communication via USB Serial.

## Features

- **Multi-threaded Architecture**: Separate threads for USB Serial communication and video recording
- **Thread-Safe Communication**: Message queues with mutex protection for inter-thread communication
- **Automatic Device Detection**: Finds CH340 USB-Serial devices automatically
- **Dual Camera Support**: Can handle 1 or 2 IP cameras simultaneously
- **Automatic Video Segmentation**: Extracts video segments based on door open/close events
- **Comprehensive Logging**: Thread-safe logging with timestamps
- **Video Processing**: Automatic resizing and color adjustment using FFmpeg

## System Requirements

- Raspberry Pi 5 (or compatible Linux system)
- C++17 compatible compiler (g++ 7.0 or higher)
- CMake 3.16 or higher
- FFmpeg installed on the system
- IP cameras accessible via RTSP
- CH340 USB-Serial device for peripheral communication

## Dependencies

Install required packages:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake g++ ffmpeg
```

## Project Structure

```
PassFlowProject/
├── include/
│   ├── Common.h           # Common definitions and utilities
│   ├── MessageQueue.h     # Thread-safe message queue
│   ├── Logger.h           # Thread-safe logging
│   ├── MainControl.h      # USB Serial communication
│   └── VideoControl.h     # Camera recording and processing
├── src/
│   ├── main.cpp          # Application entry point
│   ├── MainControl.cpp   # MainControl implementation
│   └── VideoControl.cpp  # VideoControl implementation
├── CMakeLists.txt        # Build configuration
└── config.ini.example    # Configuration file example
```

## Building the Project

1. Navigate to the project directory:
```bash
cd PassFlowProject
```

2. Create and enter build directory:
```bash
mkdir -p build
cd build
```

3. Run CMake:
```bash
cmake ..
```

4. Build the project:
```bash
make
```

5. The executable will be created as `build/passflow`

## Configuration

1. Create the PassFlow directory structure:
```bash
mkdir -p ~/PassFlow/{Log,Cam0Source,Cam0,Cam1Source,Cam1}
```

2. Copy and edit the configuration file:
```bash
cp config.ini.example ~/PassFlow/config.ini
```

3. Edit `~/PassFlow/config.ini` with your camera IP addresses and RTSP URLs

## Running the Application

1. Ensure your USB-Serial device (CH340) is connected
2. Ensure your IP cameras are accessible on the network
3. Run the application:

```bash
./build/passflow
```

4. To run as a background service:
```bash
./build/passflow > /dev/null 2>&1 &
```

5. To stop the application, press Ctrl+C or send SIGTERM:
```bash
killall passflow
```

## Architecture

### MainControl Block

**Responsibilities:**
- Discovers and connects to CH340 USB-Serial device at 115200 baud
- Receives 1-byte commands from peripheral devices
- Logs all received commands with timestamps
- Sends 1-byte commands to peripheral devices
- Tracks door open/close events and timestamps
- Sends StartStop messages to VideoControl when doors close

**Commands Received (1 byte each):**
- Door0_Open (0x01), Door0_Close (0x02)
- Door1_Open (0x03), Door1_Close (0x04)
- MainSupplyON (0x05), MainSupplyOFF (0x06)
- IgnitionON (0x07), IgnitionOFF (0x08)
- Cover0Opened (0x09), Cover0Closed (0x0A)
- Cover1Opened (0x0B), Cover1Closed (0x0C)

**Commands Sent (1 byte each):**
- LED controls: Red (0x10-0x12), Green (0x13-0x15), Blue (0x16-0x18)
- Camera controls: Cam0 (0x19-0x1A), Cam1 (0x1B-0x1C)
- Light controls: Light0 (0x1D-0x1E), Light1 (0x1F-0x20)
- Fan control: FanON (0x21), FanOFF (0x22)

### VideoControl Block

**Responsibilities:**
- Connects to IP cameras via RTSP
- Records continuous video streams using FFmpeg
- Processes StartStop messages from MainControl
- Extracts video segments based on door open/close timestamps
- Resizes and color-adjusts extracted segments
- Organizes videos by date in separate directories

**Directory Structure:**
```
~/PassFlow/
├── Log/                    # Log files
├── Cam0Source/            # Camera 0 continuous recordings
├── Cam0/                  # Camera 0 processed segments
│   └── 2025-01-26/       # Dated subdirectories
├── Cam1Source/            # Camera 1 continuous recordings
└── Cam1/                  # Camera 1 processed segments
    └── 2025-01-26/       # Dated subdirectories
```

### Thread-Safe Communication

- **Message Queues**: Implemented with mutexes and condition variables
- **Logger**: All logging operations are mutex-protected
- **Atomic Operations**: Used for shutdown signaling

## Command Protocol

### USB Serial Communication

**Format:** All commands are single byte values (0x01 - 0x22)

**Received Commands from Peripherals (1 byte each):**
```
Door0_Open     = 0x01
Door0_Close    = 0x02
Door1_Open     = 0x03
Door1_Close    = 0x04
MainSupplyON   = 0x05
MainSupplyOFF  = 0x06
IgnitionON     = 0x07
IgnitionOFF    = 0x08
Cover0Opened   = 0x09
Cover0Closed   = 0x0A
Cover1Opened   = 0x0B
Cover1Closed   = 0x0C
```

**Sent Commands to Peripherals (1 byte each):**
```
RedLedON       = 0x10
RedLedOFF      = 0x11
RedLedBlink    = 0x12
GreenLedON     = 0x13
GreenLedOFF    = 0x14
GreenLedBlink  = 0x15
BlueLedON      = 0x16
BlueLedOFF     = 0x17
BlueLedBlink   = 0x18
Cam0ON         = 0x19
Cam0OFF        = 0x1A
Cam1ON         = 0x1B
Cam1OFF        = 0x1C
Light0ON       = 0x1D
Light0OFF      = 0x1E
Light1ON       = 0x1F
Light1OFF      = 0x20
FanON          = 0x21
FanOFF         = 0x22
```

## Workflow Example

1. Door0 opens → MainControl receives byte 0x01 (Door0_Open)
2. MainControl logs event and sends 0x19 (Cam0ON), 0x1D (Light0ON) to peripherals
3. MainControl records timestamp
4. Door0 closes → MainControl receives byte 0x02 (Door0_Close)
5. MainControl sends 0x1A (Cam0OFF), 0x1E (Light0OFF) to peripherals
6. MainControl sends StartStop message to VideoControl with timestamps
7. VideoControl stops current recording and starts new one
8. VideoControl extracts segment from old file (Door0_Open to Door0_Close)
9. VideoControl resizes and processes the segment
10. Processed segment saved in ~/PassFlow/Cam0/YYYY-MM-DD/

## Troubleshooting

### Serial Port Issues

If CH340 device is not found:
```bash
# Check for USB-Serial devices
ls -l /dev/ttyUSB*

# Check kernel messages
dmesg | grep tty

# Check if ch341 driver is loaded
lsmod | grep ch341
```

### Camera Connection Issues

Test RTSP stream with FFmpeg:
```bash
ffmpeg -i "rtsp://admin:password@192.168.1.100:554/stream1" -frames:v 1 test.jpg
```

### Permission Issues

Add user to dialout group for serial port access:
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in for changes to take effect
```

### Log Files

Check logs for errors:
```bash
tail -f ~/PassFlow/Log/passflow_*.log
```

## Performance Optimization

- The application uses efficient thread communication with minimal locking
- Video processing is done in separate detached threads to avoid blocking
- FFmpeg uses hardware acceleration when available on Raspberry Pi 5

## Future Enhancements

- Configuration file parsing (currently hardcoded)
- Disk space management and automatic cleanup
- Network status monitoring
- System health metrics
- Web interface for monitoring
- Database integration for event tracking

## License

Proprietary - All rights reserved

## Support

For issues and questions, please contact the development team.
