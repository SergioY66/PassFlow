# PassFlow Changes Summary - MySqlComm Integration

## Overview

This document summarizes all changes made according to Prompt2.txt requirements.

## New Files Added

### 1. `include/MySqlComm.h`
New module header for MariaDB communication:
- `AppSettings` struct - holds all settings read from database
- `SystemStatus_t` struct - new USB protocol (bitfield for door/cover/power states)
- `EventType` enum - event types for database logging
- `MySqlComm` class - handles all database transactions

### 2. `src/MySqlComm.cpp`
Implementation of MySqlComm module:
- Connects to local MariaDB at 127.0.0.1
- Login: `bus` / Password: `njkmrjbus`
- Database: `buslocal`
- Reads settings from `settings` table
- Reads remote DB addresses from `remoteDB` table
- Logs events and video segments to database

### 3. `database_schema.sql`
SQL script to create required database tables:
- `settings` - application configuration
- `remoteDB` - remote database addresses
- `events` - door/cover/power state change log
- `video_segments` - recorded video file log

## Modified Files

### 1. `include/Common.h`
- Added `#include <cstdint>` for uint8_t
- Added `SystemStatus_t` struct with bitfields matching USB protocol
- Added helper methods: `toByte()`, `fromByte()`, comparison operators
- Kept legacy `ReceivedCommand` enum for backward compatibility

### 2. `include/MainControl.h`
- Added `#include "MySqlComm.h"`
- Added `std::shared_ptr<MySqlComm> dbComm_` member
- Added `SystemStatus_t currentStatus_` and `previousStatus_` tracking
- Added delay settings: `stopBeginDelay_`, `stopEndDelay_`
- Added new methods:
  - `processSystemStatus()` - handles new 2-byte protocol
  - `compareAndLogChanges()` - detects and logs state changes
  - `validateStatusMessage()` - validates status + ~status
  - `updateSettings()` - updates delay values from DB
- Modified constructor to accept `MySqlComm` shared pointer

### 3. `src/MainControl.cpp`
Complete rewrite of USB communication:
- **New Protocol**: Receives 2-byte messages (SystemStatus + ~SystemStatus)
- **Validation**: XOR check ensures message integrity
- **State Tracking**: Compares new status with previous, logs changes to DB
- **Video Timing**: Calculates start/stop times with delays:
  - `start_date_time = datetime(Door open) - stopBeginDelay`
  - `stop_date_time = datetime(Door close) + stopEndDelay`
- **Single Message**: Sends one message to VideoControl with both times

### 4. `include/VideoControl.h`
- Added `#include "MySqlComm.h"`
- Added `std::shared_ptr<MySqlComm> dbComm_` member
- Added `daysBeforeDeleteVideo_` setting
- Added `cleanupOldVideos()` method
- Modified constructors to accept `MySqlComm` shared pointer

### 5. `src/VideoControl.cpp`
- Reads camera configuration from database (not hardcoded)
- Uses `cam0_string` and `cam1_string` from settings
- Configures number of cameras based on `doors` setting
- Logs video segments to database
- Implements automatic cleanup of old videos based on `daysBeforeDeleteVideo`

### 6. `src/main.cpp`
- Added MySqlComm initialization before other components
- Reads all settings from database on startup
- Passes `dbComm` to MainControl and VideoControl
- Calls `mainControl->updateSettings()` with delay values
- Enabled `videoControl->start()` (was commented out)

### 7. `CMakeLists.txt`
- Added `src/MySqlComm.cpp` to source files
- Added MariaDB/MySQL client library detection via pkg-config
- Added include directories and link libraries for MariaDB

## USB Protocol Change

### Old Protocol (Legacy)
- Single byte commands (0x01-0x0C)
- Separate messages for each event

### New Protocol
```c
typedef struct {
    uint8_t door_0      : 1;    // Door 0: 0=OPENED, 1=CLOSED
    uint8_t door_1      : 1;    // Door 1: 0=OPENED, 1=CLOSED
    uint8_t cover_0     : 1;    // Cover 0: 0=OPENED, 1=CLOSED
    uint8_t cover_1     : 1;    // Cover 1: 0=OPENED, 1=CLOSED
    uint8_t mainSupply  : 1;    // Main supply: 0=OFF, 1=ON
    uint8_t ignition    : 1;    // Ignition: 0=OFF, 1=ON
    uint8_t reserved    : 2;    // Reserved
} SystemStatus_t;
```
- Sent as 2 bytes: `[SystemStatus][~SystemStatus]`
- Validation: `status XOR invStatus == 0xFF`

## Message Protocol Change

### Old Protocol
Two separate messages:
1. "Door Open + datetime"
2. "Door Close + datetime"

### New Protocol
Single message with adjusted times:
- `start_date_time = datetime(Door open) - stopBeginDelay`
- `stop_date_time = datetime(Door close) + stopEndDelay`

## Database Schema

### settings table
| Field | Type | Description |
|-------|------|-------------|
| doors | INT | Number of doors/cameras |
| stopBeginDelay | INT | Seconds before door open |
| stopEndDelay | INT | Seconds after door close |
| daysBeforeDeliteVideo | INT | Days to keep videos |
| cam0_string | VARCHAR | Camera 0 connection string |
| cam1_string | VARCHAR | Camera 1 connection string |

### remoteDB table
| Field | Type | Description |
|-------|------|-------------|
| remoteDBAddress | VARCHAR | Remote DB address |

### events table
| Field | Type | Description |
|-------|------|-------------|
| event_type | INT | Event type code |
| event_description | VARCHAR | "Door 0 open", etc. |
| event_time | VARCHAR | Timestamp |

### video_segments table
| Field | Type | Description |
|-------|------|-------------|
| camera_id | INT | Camera ID (0 or 1) |
| start_time | VARCHAR | Segment start time |
| stop_time | VARCHAR | Segment stop time |
| filename | VARCHAR | Path to video file |

## Build Requirements

Install MariaDB development libraries:
```bash
# Debian/Ubuntu
sudo apt-get install libmariadb-dev

# Or for MySQL
sudo apt-get install libmysqlclient-dev
```

## Database Setup

1. Install MariaDB server
2. Run database_schema.sql as root:
```bash
sudo mysql < database_schema.sql
```

3. Create user and grant permissions:
```sql
CREATE USER 'bus'@'localhost' IDENTIFIED BY 'njkmrjbus';
GRANT ALL PRIVILEGES ON buslocal.* TO 'bus'@'localhost';
FLUSH PRIVILEGES;
```

## Building

```bash
cd PassFlowClaude
mkdir -p build
cd build
cmake ..
make
```
