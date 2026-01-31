# PassFlow Project - 1-Byte Command Protocol Changes

## Summary

All peripheral communication has been updated to use **1-byte commands** instead of text-based commands. Both received commands from peripherals and sent commands to peripherals now use single byte values.

## Modified Files

### 1. include/Common.h
**Changes:**
- Updated `ReceivedCommand` enum to use `uint8_t` values (0x01-0x0C, 0xFF for Unknown)
- Updated `PeripheralCommand` enum byte values to range 0x10-0x22 (to avoid overlap)

**New Command Mappings:**

**Received Commands (from peripherals):**
```cpp
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
Unknown        = 0xFF
```

**Sent Commands (to peripherals):**
```cpp
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

### 2. include/MainControl.h
**Changes:**
- Removed `std::map<std::string, ReceivedCommand> commandMap_`
- Removed `std::map<ReceivedCommand, std::string> commandToString_`
- Replaced `ReceivedCommand parseCommand(const std::string& cmdStr)` with `std::string getCommandName(ReceivedCommand cmd)`

### 3. src/MainControl.cpp
**Changes:**
- **Constructor**: Removed initialization of string-to-command maps
- **receiverLoop()**: 
  - Changed from text string parsing to direct byte reading
  - Reads bytes directly and casts to `ReceivedCommand`
  - Validates command is in range 0x01-0x0C
  - Logs with hex format: "Received command: 0x01 (Door0_Open)"
- **parseCommand()**: Replaced with `getCommandName()` helper function
  - Returns human-readable name for a command byte
- **senderLoop()**: Improved hex logging format for sent bytes

### 4. test_serial.py
**Changes:**
- **PeripheralSimulator class**:
  - Changed `COMMANDS` from list to dictionary mapping names to byte values
  - Updated `send_command()` to send single byte instead of text string
- **decode_received_byte()**: Updated byte mappings to match new 0x10-0x22 range
- **main()**: Updated to work with dictionary-based command structure

### 5. README.md
**Changes:**
- Updated "Command Protocol" section with complete byte mapping tables
- Updated "Workflow Example" to show hex byte values
- Updated "MainControl Block" description with hex values

### 6. API_DOCUMENTATION.md
**Changes:**
- Updated `ReceivedCommand` enum documentation to show byte values
- Updated `PeripheralCommand` enum with new byte range (0x10-0x22)
- Added note that received commands are now 1-byte values

## Protocol Changes Summary

### Before (Text-Based)
- Peripheral sends: `"Door0_Open\n"` (ASCII text + newline)
- PassFlow sends: Single bytes (0x01-0x13)

### After (All 1-Byte)
- Peripheral sends: `0x01` (Door0_Open)
- PassFlow sends: Single bytes (0x10-0x22)

## Communication Flow

```
Peripheral Device         PassFlow System
      |                        |
      |------- 0x01 ---------->|  (Door0_Open received)
      |                        |
      |<------ 0x19 -----------|  (Cam0ON sent)
      |<------ 0x1D -----------|  (Light0ON sent)
      |                        |
      |------- 0x02 ---------->|  (Door0_Close received)
      |                        |
      |<------ 0x1A -----------|  (Cam0OFF sent)
      |<------ 0x1E -----------|  (Light0OFF sent)
```

## Benefits

1. **Simpler Protocol**: Single byte for all commands, no text parsing needed
2. **More Efficient**: Reduced bandwidth and processing overhead
3. **Consistent**: Both directions use the same byte-based protocol
4. **No Ambiguity**: Fixed command codes, no string matching issues
5. **Easier Debugging**: Clear hex values in logs

## Testing

Use the updated `test_serial.py` script to test:

```bash
python3 test_serial.py /dev/ttyUSB0
```

The script now sends 1-byte commands and correctly decodes received bytes.

## Compilation

No changes to build process. Build as before:

```bash
./build.sh
```

Or manually:
```bash
mkdir -p build && cd build
cmake ..
make
```

## Backward Compatibility

⚠️ **Important**: This change is NOT backward compatible with peripherals expecting text commands. Peripheral devices must be updated to send/receive 1-byte commands as specified above.
