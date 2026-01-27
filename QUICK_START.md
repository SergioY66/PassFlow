# PassFlow Quick Start Guide

## 🚀 Getting Started in 5 Minutes

### Step 1: Prerequisites
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ffmpeg python3-serial
```

### Step 2: Extract and Build
```bash
cd PassFlowProject
./build.sh
```

### Step 3: Configure
```bash
# Edit camera settings
nano ~/PassFlow/config.ini

# Update these lines with your camera IPs:
# [Camera0]
# RTSPUrl = rtsp://admin:password@YOUR_CAMERA_IP:554/stream1
```

### Step 4: Test Serial Connection (Optional)
```bash
# Find your USB-Serial device
ls -l /dev/ttyUSB*

# Test with simulator
python3 test_serial.py /dev/ttyUSB0
```

### Step 5: Run
```bash
./build/passflow
```

## 📋 Command Reference

### Build Commands
```bash
./build.sh              # Automated build
make                    # Alternative build
make clean              # Clean build
make debug              # Debug build
```

### Installation Commands
```bash
./install.sh                  # Install locally
sudo ./install.sh --service   # Install as system service
```

### Service Commands
```bash
sudo systemctl start passflow    # Start service
sudo systemctl stop passflow     # Stop service
sudo systemctl status passflow   # Check status
sudo systemctl enable passflow   # Enable at boot
sudo systemctl disable passflow  # Disable at boot
```

### Monitoring Commands
```bash
# View application logs
tail -f ~/PassFlow/Log/passflow_*.log

# View service logs
sudo journalctl -u passflow -f

# Check for USB devices
ls -l /dev/ttyUSB*

# Test camera connection
ffmpeg -i "rtsp://admin:password@192.168.1.100:554/stream1" -frames:v 1 test.jpg
```

## 🔍 Troubleshooting

### Issue: "CH340 device not found"
```bash
# Check USB devices
lsusb

# Check kernel modules
lsmod | grep ch341

# Check serial devices
dmesg | grep tty
```

### Issue: "Permission denied on /dev/ttyUSB0"
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Log out and log back in
```

### Issue: "Cannot connect to camera"
```bash
# Test camera with FFmpeg
ffmpeg -i "rtsp://admin:password@192.168.1.100:554/stream1" -frames:v 1 test.jpg

# Check network connectivity
ping 192.168.1.100
```

### Issue: "FFmpeg not found"
```bash
# Install FFmpeg
sudo apt-get install -y ffmpeg

# Verify installation
which ffmpeg
ffmpeg -version
```

## 📁 Directory Structure After Installation

```
~/PassFlow/
├── Log/                          # All log files
│   └── passflow_2025-01-26.log
├── Cam0Source/                   # Camera 0 raw recordings
│   └── 20250126_143052_cam0.mp4
├── Cam0/                         # Camera 0 processed segments
│   └── 2025-01-26/
│       └── 2025-01-26_14-30-52_2025-01-26_14-31-15.mp4
├── Cam1Source/                   # Camera 1 raw recordings
└── Cam1/                         # Camera 1 processed segments
    └── 2025-01-26/
```

## 🔧 Configuration File (~/.PassFlow/config.ini)

```ini
[Camera0]
Enabled = true
IPAddress = 192.168.1.100
RTSPUrl = rtsp://admin:password@192.168.1.100:554/stream1

[Camera1]
Enabled = false
IPAddress = 192.168.1.101
RTSPUrl = rtsp://admin:password@192.168.1.101:554/stream1
```

## 🧪 Testing Commands

### Test Serial Communication
```bash
# Interactive testing
python3 test_serial.py /dev/ttyUSB0

# In the menu:
# Press 1 for Door0_Open
# Press 2 for Door0_Close
# Press 20 for complete Door0 cycle
# Press r to monitor received bytes
```

### Test Video Recording
```bash
# Check if recording is active
ps aux | grep ffmpeg

# Check recorded files
ls -lh ~/PassFlow/Cam0Source/

# Check processed segments
ls -lh ~/PassFlow/Cam0/$(date +%Y-%m-%d)/
```

## 📊 System Status Checks

```bash
# Check process
ps aux | grep passflow

# Check CPU usage
top -p $(pgrep passflow)

# Check disk usage
df -h ~/PassFlow

# Check memory usage
free -h
```

## 🛑 Stopping the Application

### If running in terminal
```
Press Ctrl+C
```

### If running as service
```bash
sudo systemctl stop passflow
```

### If running in background
```bash
killall passflow
```

## 📞 Getting Help

1. Check log files: `tail -f ~/PassFlow/Log/*.log`
2. Check service logs: `sudo journalctl -u passflow -f`
3. Review README.md for detailed information
4. Review API_DOCUMENTATION.md for technical details

## ⚡ Quick Test Workflow

```bash
# 1. Build
./build.sh

# 2. Run
./build/passflow &

# 3. Test in another terminal
python3 test_serial.py /dev/ttyUSB0

# 4. Send Door0_Open (option 1)
# 5. Wait 5 seconds
# 6. Send Door0_Close (option 2)

# 7. Check logs
tail ~/PassFlow/Log/*.log

# 8. Check video segment created
ls -lh ~/PassFlow/Cam0/$(date +%Y-%m-%d)/

# 9. Stop
killall passflow
```

## 🎯 Common Use Cases

### Use Case 1: Deploy on Raspberry Pi
```bash
scp -r PassFlowProject pi@raspberrypi:~/
ssh pi@raspberrypi
cd ~/PassFlowProject
./build.sh
sudo ./install.sh --service
```

### Use Case 2: Development Testing
```bash
make debug
gdb ./build/passflow
```

### Use Case 3: Monitor System
```bash
# Terminal 1: Run application
./build/passflow

# Terminal 2: Monitor logs
tail -f ~/PassFlow/Log/*.log

# Terminal 3: Send test commands
python3 test_serial.py /dev/ttyUSB0
```

## ✅ Verification Checklist

- [ ] FFmpeg installed: `which ffmpeg`
- [ ] Serial port accessible: `ls -l /dev/ttyUSB*`
- [ ] User in dialout group: `groups | grep dialout`
- [ ] Cameras accessible: `ping YOUR_CAMERA_IP`
- [ ] Directory structure created: `ls -l ~/PassFlow/`
- [ ] Build successful: `ls -l build/passflow`
- [ ] Application runs: `./build/passflow` (Ctrl+C to stop)
- [ ] Logs created: `ls -l ~/PassFlow/Log/`

## 🎉 Success Indicators

When everything is working correctly, you should see:

1. **Console output**: "PassFlow System running. Press Ctrl+C to stop."
2. **Log file**: Created in `~/PassFlow/Log/` with current date
3. **FFmpeg processes**: Visible in `ps aux | grep ffmpeg`
4. **Video files**: Created in `~/PassFlow/Cam0Source/`
5. **Serial communication**: Commands logged when sent from test utility
6. **Processed segments**: Created in `~/PassFlow/Cam0/DATE/` after door cycle

---

**Ready to go!** Your PassFlow system is now operational. 🎊
