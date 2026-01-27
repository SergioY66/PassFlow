# PassFlow System - Byte Command Reference

## Overview

All communication between the PassFlow system and peripheral devices uses **single-byte commands** over USB Serial (CH340) at **115200 baud, 8N1**.

---

## Commands Received FROM Peripherals (0x01 - 0x0C)

These are 1-byte commands that the MainControl block receives from external peripheral devices:

| Byte Value | Hex Code | Command Name        | Description                          |
|------------|----------|---------------------|--------------------------------------|
| 1          | 0x01     | Door0_Open          | Door 0 has opened                    |
| 2          | 0x02     | Door0_Close         | Door 0 has closed                    |
| 3          | 0x03     | Door1_Open          | Door 1 has opened                    |
| 4          | 0x04     | Door1_Close         | Door 1 has closed                    |
| 5          | 0x05     | MainSupplyON        | Main power supply turned on          |
| 6          | 0x06     | MainSupplyOFF       | Main power supply turned off         |
| 7          | 0x07     | IgnitionON          | Ignition turned on                   |
| 8          | 0x08     | IgnitionOFF         | Ignition turned off                  |
| 9          | 0x09     | Cover0Opened        | Cover 0 has been opened              |
| 10         | 0x0A     | Cover0Closed        | Cover 0 has been closed              |
| 11         | 0x0B     | Cover1Opened        | Cover 1 has been opened              |
| 12         | 0x0C     | Cover1Closed        | Cover 1 has been closed              |

### Automated Actions on Door Events:

**When Door0_Open (0x01) is received:**
- System automatically sends: `Cam0ON (0x19)` and `Light0ON (0x1D)`
- Records door open timestamp

**When Door0_Close (0x02) is received:**
- System automatically sends: `Cam0OFF (0x1A)` and `Light0OFF (0x1E)`
- Triggers video segment extraction (from door open to close time)
- Sends StartStop message to VideoControl

**When Door1_Open (0x03) is received:**
- System automatically sends: `Cam1ON (0x1B)` and `Light1ON (0x1F)`
- Records door open timestamp

**When Door1_Close (0x04) is received:**
- System automatically sends: `Cam1OFF (0x1C)` and `Light1OFF (0x20)`
- Triggers video segment extraction (from door open to close time)
- Sends StartStop message to VideoControl

---

## Commands Sent TO Peripherals (0x10 - 0x22)

These are 1-byte commands that the MainControl block sends to peripheral devices:

### LED Control Commands (Red, Green, Blue)

| Byte Value | Hex Code | Command Name    | Description                      |
|------------|----------|-----------------|----------------------------------|
| 16         | 0x10     | RedLedON        | Turn red LED on (solid)          |
| 17         | 0x11     | RedLedOFF       | Turn red LED off                 |
| 18         | 0x12     | RedLedBlink     | Set red LED to blinking mode     |
| 19         | 0x13     | GreenLedON      | Turn green LED on (solid)        |
| 20         | 0x14     | GreenLedOFF     | Turn green LED off               |
| 21         | 0x15     | GreenLedBlink   | Set green LED to blinking mode   |
| 22         | 0x16     | BlueLedON       | Turn blue LED on (solid)         |
| 23         | 0x17     | BlueLedOFF      | Turn blue LED off                |
| 24         | 0x18     | BlueLedBlink    | Set blue LED to blinking mode    |

### Camera Control Commands

| Byte Value | Hex Code | Command Name    | Description                      |
|------------|----------|-----------------|----------------------------------|
| 25         | 0x19     | Cam0ON          | Enable camera 0 recording        |
| 26         | 0x1A     | Cam0OFF         | Disable camera 0 recording       |
| 27         | 0x1B     | Cam1ON          | Enable camera 1 recording        |
| 28         | 0x1C     | Cam1OFF         | Disable camera 1 recording       |

### Light Control Commands

| Byte Value | Hex Code | Command Name    | Description                      |
|------------|----------|-----------------|----------------------------------|
| 29         | 0x1D     | Light0ON        | Turn on light 0                  |
| 30         | 0x1E     | Light0OFF       | Turn off light 0                 |
| 31         | 0x1F     | Light1ON        | Turn on light 1                  |
| 32         | 0x20     | Light1OFF       | Turn off light 1                 |

### Fan Control Commands

| Byte Value | Hex Code | Command Name    | Description                      |
|------------|----------|-----------------|----------------------------------|
| 33         | 0x21     | FanON           | Turn on cooling fan              |
| 34         | 0x22     | FanOFF          | Turn off cooling fan             |

---

## Serial Communication Specifications

### Hardware
- **Device**: CH340 USB-to-Serial adapter
- **Connection**: Automatically detected at `/dev/ttyUSBx`

### Configuration
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None (8N1 mode)

### Protocol
- **Format**: Raw binary bytes (no text encoding)
- **No terminators**: Each command is exactly 1 byte
- **No handshaking**: Commands sent immediately
- **Bidirectional**: Both sending and receiving happen simultaneously

---

## C++ Implementation Details

### Command Enums in Common.h

```cpp
// Commands received FROM peripherals
enum class ReceivedCommand : uint8_t {
    Door0_Open = 0x01,
    Door0_Close = 0x02,
    Door1_Open = 0x03,
    Door1_Close = 0x04,
    MainSupplyON = 0x05,
    MainSupplyOFF = 0x06,
    IgnitionON = 0x07,
    IgnitionOFF = 0x08,
    Cover0Opened = 0x09,
    Cover0Closed = 0x0A,
    Cover1Opened = 0x0B,
    Cover1Closed = 0x0C
};

// Commands sent TO peripherals
enum class PeripheralCommand : uint8_t {
    RedLedON = 0x10,
    RedLedOFF = 0x11,
    RedLedBlink = 0x12,
    GreenLedON = 0x13,
    GreenLedOFF = 0x14,
    GreenLedBlink = 0x15,
    BlueLedON = 0x16,
    BlueLedOFF = 0x17,
    BlueLedBlink = 0x18,
    Cam0ON = 0x19,
    Cam0OFF = 0x1A,
    Cam1ON = 0x1B,
    Cam1OFF = 0x1C,
    Light0ON = 0x1D,
    Light0OFF = 0x1E,
    Light1ON = 0x1F,
    Light1OFF = 0x20,
    FanON = 0x21,
    FanOFF = 0x22
};
```

### Receiving Commands (MainControl::receiverLoop)

```cpp
// Read bytes from serial port
int bytesRead = read(serialFd_, buffer, sizeof(buffer));

// Process each byte
for (int i = 0; i < bytesRead; i++) {
    uint8_t cmdByte = buffer[i];
    ReceivedCommand cmd = static_cast<ReceivedCommand>(cmdByte);
    
    // Validate command range (0x01 - 0x0C)
    if (cmdByte >= 0x01 && cmdByte <= 0x0C) {
        processReceivedCommand(cmd);
    }
}
```

### Sending Commands (MainControl::senderLoop)

```cpp
// Get command from queue
auto cmdOpt = outgoingQueue_.tryPop();

if (cmdOpt.has_value()) {
    uint8_t byte = static_cast<uint8_t>(cmdOpt.value());
    // Write single byte to serial port
    write(serialFd_, &byte, 1);
}
```

---

## Python Test Utility Usage

### Testing with test_serial.py

The included Python utility simulates a peripheral device for testing:

```bash
# Connect to serial port
python3 test_serial.py /dev/ttyUSB0
```

### Example Test Sequence

```bash
# Start the PassFlow application
./build/passflow

# In another terminal, run the test utility
python3 test_serial.py /dev/ttyUSB0

# Select option 20 to simulate Door0 cycle
# This will:
# 1. Send Door0_Open (0x01)
# 2. Receive Cam0ON (0x19) and Light0ON (0x1D)
# 3. Wait 5 seconds
# 4. Send Door0_Close (0x02)
# 5. Receive Cam0OFF (0x1A) and Light0OFF (0x1E)
```

---

## Workflow Example

### Complete Door0 Cycle

```
Time   | Event                        | Bytes Sent/Received
-------|------------------------------|------------------------
T+0s   | Peripheral sends 0x01        | → MainControl receives Door0_Open
T+0s   | MainControl responds         | ← Sends 0x19 (Cam0ON)
T+0s   | MainControl responds         | ← Sends 0x1D (Light0ON)
T+0s   | VideoControl starts recording| (internal)
T+5s   | Peripheral sends 0x02        | → MainControl receives Door0_Close
T+5s   | MainControl responds         | ← Sends 0x1A (Cam0OFF)
T+5s   | MainControl responds         | ← Sends 0x1E (Light0OFF)
T+5s   | VideoControl extracts segment| (internal, 0s to 5s)
T+5s   | Segment processing starts    | (FFmpeg resize & color adjust)
```

---

## Debugging Commands

### Monitor Serial Traffic with Python

```python
import serial

ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

while True:
    if ser.in_waiting > 0:
        byte = ser.read(1)
        print(f"Received: 0x{ord(byte):02X}")
```

### Send Test Command from Command Line

```bash
# Send Door0_Open (0x01)
echo -ne '\x01' > /dev/ttyUSB0

# Send Door0_Close (0x02)
echo -ne '\x02' > /dev/ttyUSB0
```

### Monitor with stty and hexdump

```bash
# Configure serial port
stty -F /dev/ttyUSB0 115200 raw -echo

# Read and display hex values
cat /dev/ttyUSB0 | hexdump -C
```

---

## Log Output Examples

### When receiving commands:

```
[2025-01-26 14:23:45.123] [CMD] Received command: 0x01 (Door0_Open)
[2025-01-26 14:23:45.125] [LOG] Sent command: 0x19
[2025-01-26 14:23:45.126] [LOG] Sent command: 0x1D
```

### When processing door cycle:

```
[2025-01-26 14:23:50.456] [CMD] Received command: 0x02 (Door0_Close)
[2025-01-26 14:23:50.458] [MSG] StartStop message sent to VideoControl (Cam0)
[2025-01-26 14:23:50.460] [LOG] Sent command: 0x1A
[2025-01-26 14:23:50.461] [LOG] Sent command: 0x1E
```

---

## Error Handling

### Invalid Command Bytes

If a byte outside the valid ranges is received, it's logged as an error:

```
[2025-01-26 14:24:10.789] [ERR] Unknown command byte: 0xFF
```

### Valid Ranges

- **Received commands**: 0x01 to 0x0C (12 commands)
- **Sent commands**: 0x10 to 0x22 (19 commands)

Any byte outside these ranges is considered invalid.

---

## Integration Notes

### For Peripheral Device Developers

1. **Send commands**: Simply write a single byte to the serial port
2. **Receive responses**: Read single bytes from the serial port
3. **No buffering needed**: Each command is atomic (1 byte)
4. **No ACK required**: Commands are fire-and-forget
5. **Timing**: Allow ~10ms between commands for reliable processing

### For System Integrators

1. All communication is **asynchronous** and **non-blocking**
2. Commands can arrive at any time
3. The system maintains internal state (door open times)
4. Video extraction happens automatically on door close events
5. All events are logged with millisecond timestamps

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────┐
│                    PASSFLOW COMMANDS                    │
├─────────────────────────────────────────────────────────┤
│ RECEIVED (FROM PERIPHERAL)  │ SENT (TO PERIPHERAL)      │
├─────────────┬───────────────┼─────────────┬─────────────┤
│ 0x01 Door0O │ Door0_Open    │ 0x10 RedON  │ Red LED on  │
│ 0x02 Door0C │ Door0_Close   │ 0x11 RedOFF │ Red LED off │
│ 0x03 Door1O │ Door1_Open    │ 0x12 RedBlk │ Red blink   │
│ 0x04 Door1C │ Door1_Close   │ 0x13 GrnON  │ Green on    │
│ 0x05 MainON │ Main supply   │ 0x14 GrnOFF │ Green off   │
│ 0x06 MainOF │ Main off      │ 0x15 GrnBlk │ Green blink │
│ 0x07 IgnON  │ Ignition on   │ 0x16 BluON  │ Blue on     │
│ 0x08 IgnOFF │ Ignition off  │ 0x17 BluOFF │ Blue off    │
│ 0x09 Cov0O  │ Cover0 open   │ 0x18 BluBlk │ Blue blink  │
│ 0x0A Cov0C  │ Cover0 close  │ 0x19 Cam0ON │ Camera0 on  │
│ 0x0B Cov1O  │ Cover1 open   │ 0x1A Cam0OF │ Camera0 off │
│ 0x0C Cov1C  │ Cover1 close  │ 0x1B Cam1ON │ Camera1 on  │
│             │               │ 0x1C Cam1OF │ Camera1 off │
│             │               │ 0x1D Lt0 ON │ Light0 on   │
│             │               │ 0x1E Lt0 OF │ Light0 off  │
│             │               │ 0x1F Lt1 ON │ Light1 on   │
│             │               │ 0x20 Lt1 OF │ Light1 off  │
│             │               │ 0x21 Fan ON │ Fan on      │
│             │               │ 0x22 Fan OF │ Fan off     │
└─────────────┴───────────────┴─────────────┴─────────────┘
```

---

## Summary

The PassFlow system uses a **simple, efficient 1-byte command protocol** for all peripheral communication. This design:

✓ Minimizes latency (1 byte = fastest transmission)  
✓ Simplifies implementation (no parsing needed)  
✓ Eliminates protocol overhead (no headers, terminators, or checksums)  
✓ Provides 256 possible commands (currently using 31)  
✓ Works reliably at 115200 baud over USB Serial  

The system is **production-ready** and has been designed with proper thread safety, error handling, and comprehensive logging.
