# PassFlow API Documentation

## Overview

This document describes the internal APIs and interfaces of the PassFlow system.

## Thread Architecture

### Thread 1: MainControl Receiver
- **Purpose**: Receive commands from peripheral devices via USB Serial
- **Communication**: Writes to `videoControlQueue` message queue
- **Blocking**: Non-blocking I/O with 10ms sleep intervals
- **Safety**: Uses atomic `running_` flag for shutdown coordination

### Thread 2: MainControl Sender
- **Purpose**: Send commands to peripheral devices via USB Serial
- **Communication**: Reads from `outgoingQueue` message queue
- **Blocking**: Blocks on queue pop with 100ms timeout
- **Safety**: Uses atomic `running_` flag for shutdown coordination

### Thread 3: VideoControl Message Handler
- **Purpose**: Process StartStop messages from MainControl
- **Communication**: Reads from `videoControlQueue` message queue
- **Blocking**: Blocks on queue pop with 100ms timeout
- **Safety**: Uses atomic `running_` flag for shutdown coordination

### Thread 4+: Camera Recorders (one per camera)
- **Purpose**: Manage FFmpeg recording process for each camera
- **Communication**: Processes StartStop messages directly
- **Blocking**: 1-second sleep intervals with FFmpeg process monitoring
- **Safety**: Uses atomic `running_` flag and mutex-protected file operations

### Detached Threads: Video Segment Processing
- **Purpose**: Extract and process video segments
- **Communication**: None (fire-and-forget)
- **Lifetime**: Created on-demand, exits when processing complete
- **Safety**: Each operates on independent files

## Message Queue API

### Template Class: `MessageQueue<T>`

Thread-safe FIFO queue for inter-thread communication.

#### Methods

```cpp
void push(const T& message)
void push(T&& message)
```
- Adds message to queue
- Thread-safe
- Non-blocking
- Notifies waiting threads

```cpp
std::optional<T> pop()
```
- Retrieves and removes message from queue
- Thread-safe
- Blocks until message available or shutdown
- Returns `std::nullopt` on shutdown

```cpp
std::optional<T> tryPop(std::chrono::milliseconds timeout)
```
- Retrieves and removes message from queue
- Thread-safe
- Blocks up to timeout duration
- Returns `std::nullopt` on timeout or shutdown

```cpp
bool empty() const
size_t size() const
```
- Query queue state
- Thread-safe
- Non-blocking

```cpp
void requestShutdown()
bool isShutdown() const
```
- Coordinate graceful shutdown
- Wakes all waiting threads
- Thread-safe

## Logger API

### Class: `Logger`

Thread-safe logging with automatic timestamping.

#### Constructor

```cpp
Logger(const std::string& logDir = "~/PassFlow/Log")
```
- Creates log directory if needed
- Opens log file with current date
- Throws `std::runtime_error` on failure

#### Methods

```cpp
void logCommand(const std::string& command)
```
- Logs peripheral commands with timestamp
- Thread-safe
- Auto-flushes

```cpp
void log(const std::string& message)
```
- Logs general messages with timestamp
- Thread-safe
- Auto-flushes

```cpp
void logError(const std::string& error)
```
- Logs errors with "ERROR:" prefix
- Thread-safe
- Auto-flushes

## MainControl API

### Class: `MainControl`

Manages USB Serial communication with peripheral devices.

#### Constructor

```cpp
MainControl(std::shared_ptr<Logger> logger,
           std::shared_ptr<MessageQueue<Message>> videoControlQueue)
```
- Initializes command mappings
- Does not start threads

#### Public Methods

```cpp
bool initialize()
```
- Finds CH340 device
- Opens and configures serial port
- Returns false on failure

```cpp
void start()
```
- Starts receiver and sender threads
- Begins processing commands

```cpp
void stop()
```
- Gracefully stops all threads
- Closes serial port
- Waits for thread termination

```cpp
void sendCommand(PeripheralCommand cmd)
```
- Queues command to send to peripheral
- Non-blocking
- Thread-safe

## VideoControl API

### Class: `VideoControl`

Manages camera recording and video segment processing.

#### Constructor

```cpp
VideoControl(std::shared_ptr<Logger> logger,
            std::shared_ptr<MessageQueue<Message>> messageQueue)
```
- Stores references to logger and message queue
- Does not start threads

#### Public Methods

```cpp
bool initialize()
```
- Loads camera configuration
- Creates camera recorder instances
- Returns false if no cameras configured

```cpp
void start()
```
- Starts all camera recorders
- Starts message processing thread
- Begins recording

```cpp
void stop()
```
- Stops all camera recorders
- Gracefully stops message thread
- Waits for thread termination

### Class: `CameraRecorder`

Manages individual camera recording.

#### Constructor

```cpp
CameraRecorder(const CameraConfig& config, std::shared_ptr<Logger> logger)
```
- Creates source and output directories
- Configures FFmpeg parameters

#### Public Methods

```cpp
void start()
```
- Starts recording thread
- Begins FFmpeg capture

```cpp
void stop()
```
- Stops FFmpeg process
- Terminates recording thread

```cpp
void processStartStopMessage(const StartStopMessage& msg)
```
- Restarts recording with new file
- Spawns detached thread to extract segment
- Non-blocking

## Message Types

### Enum: `MessageType`

```cpp
enum class MessageType {
    StartStop,       // Video segment extraction request
    PeripheralCommand,  // Command to peripheral device
    Shutdown        // Shutdown signal
};
```

### Struct: `StartStopMessage`

```cpp
struct StartStopMessage {
    int cameraId;  // 0 or 1
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point stopTime;
};
```

### Struct: `Message`

Union-based message container with factory methods:

```cpp
static Message createStartStop(int camId,
                               std::chrono::system_clock::time_point start,
                               std::chrono::system_clock::time_point stop)

static Message createPeripheralCommand(PeripheralCommand cmd)

static Message createShutdown()
```

## Command Enumerations

### Enum: `PeripheralCommand`

Commands sent to peripheral (1 byte each):

```cpp
enum class PeripheralCommand : uint8_t {
    RedLedON = 0x10,     RedLedOFF = 0x11,     RedLedBlink = 0x12,
    GreenLedON = 0x13,   GreenLedOFF = 0x14,   GreenLedBlink = 0x15,
    BlueLedON = 0x16,    BlueLedOFF = 0x17,    BlueLedBlink = 0x18,
    Cam0ON = 0x19,       Cam0OFF = 0x1A,
    Cam1ON = 0x1B,       Cam1OFF = 0x1C,
    Light0ON = 0x1D,     Light0OFF = 0x1E,
    Light1ON = 0x1F,     Light1OFF = 0x20,
    FanON = 0x21,        FanOFF = 0x22
};
```

### Enum: `ReceivedCommand`

Commands received from peripheral (1 byte each):

```cpp
enum class ReceivedCommand : uint8_t {
    Door0_Open = 0x01,      Door0_Close = 0x02,
    Door1_Open = 0x03,      Door1_Close = 0x04,
    MainSupplyON = 0x05,    MainSupplyOFF = 0x06,
    IgnitionON = 0x07,      IgnitionOFF = 0x08,
    Cover0Opened = 0x09,    Cover0Closed = 0x0A,
    Cover1Opened = 0x0B,    Cover1Closed = 0x0C,
    Unknown = 0xFF
};
```

## Utility Functions

### Time Formatting

```cpp
std::string formatTimestamp(const std::chrono::system_clock::time_point& tp)
```
- Formats timestamp as "YYYY-MM-DD HH:MM:SS.mmm"
- Includes milliseconds

```cpp
std::string getCurrentDateString()
```
- Returns current date as "YYYY-MM-DD"

```cpp
std::string getDatetimeFilename(const std::string& suffix)
```
- Returns datetime filename as "YYYYMMDD_HHMMSS_suffix"

## Video Processing

### FFmpeg Parameters

**Recording (continuous):**
```bash
ffmpeg -rtsp_transport tcp 
       -i "rtsp://..." 
       -c:v copy 
       -c:a copy 
       -f mp4 
       -y "output.mp4"
```

**Segment Extraction:**
```bash
ffmpeg -i "source.mp4" 
       -ss <start_offset> 
       -t <duration> 
       -vf "scale=640:480,hue=s=0.8" 
       -c:v libx264 
       -preset fast 
       -crf 23 
       -c:a copy 
       -y "output.mp4"
```

## Error Handling

- Serial port errors: Logged, operations continue
- FFmpeg failures: Logged, automatic restart attempted
- File I/O errors: Logged, operation skipped
- Configuration errors: Fatal, application exits
- Thread exceptions: Caught and logged, thread exits

## Thread Safety Guarantees

1. **Logger**: All methods use mutex locking
2. **MessageQueue**: All operations are mutex-protected with condition variable synchronization
3. **File Operations**: Mutex-protected during file name generation and FFmpeg process management
4. **Atomic Flags**: Used for shutdown coordination across threads
5. **No Shared Mutable State**: Threads communicate only through message queues

## Performance Characteristics

- **Message Queue**: O(1) push/pop operations
- **Serial I/O**: 10ms polling interval (reduces CPU usage)
- **Video Processing**: Detached threads prevent blocking main operations
- **FFmpeg**: Copy mode for continuous recording (minimal CPU)
- **Segment Processing**: Hardware acceleration when available

## Testing Considerations

1. **Serial Port Testing**: Use virtual serial ports or hardware loopback
2. **Camera Testing**: Use RTSP test streams or recorded files
3. **Message Queue Testing**: Unit tests with multiple producer/consumer threads
4. **Video Processing Testing**: Test with known video files and verify output
5. **Integration Testing**: Full system test with all components

## Debug Mode

Compile with `-DDEBUG` to enable additional logging:
```bash
make debug
```
