#!/bin/bash

# PassFlow Installation Script

set -e

echo "================================"
echo "PassFlow System Installation"
echo "================================"
echo ""

# Check if running as root for service installation
if [ "$EUID" -ne 0 ] && [ "$1" == "--service" ]; then
    echo "Please run with sudo for service installation"
    exit 1
fi

# Build the project first
echo "Building project..."
./build.sh

# Copy configuration example
if [ ! -f ~/PassFlow/config.ini ]; then
    echo "Creating default configuration..."
    cp config.ini.example ~/PassFlow/config.ini
    echo "Configuration file created at ~/PassFlow/config.ini"
    echo "Please edit this file with your camera settings"
else
    echo "Configuration file already exists"
fi

# Add user to dialout group if not already
if ! groups $USER | grep -q dialout; then
    echo "Adding $USER to dialout group for serial port access..."
    sudo usermod -a -G dialout $USER
    echo "WARNING: You need to log out and log back in for group changes to take effect"
fi

# Install as systemd service if requested
if [ "$1" == "--service" ]; then
    echo ""
    echo "Installing systemd service..."
    
    # Update service file with actual user
    sed "s|User=pi|User=$SUDO_USER|g" passflow.service > /tmp/passflow.service
    sed -i "s|Group=pi|Group=$SUDO_USER|g" /tmp/passflow.service
    sed -i "s|/home/pi|$HOME|g" /tmp/passflow.service
    
    # Copy service file
    sudo cp /tmp/passflow.service /etc/systemd/system/
    rm /tmp/passflow.service
    
    # Reload systemd
    sudo systemctl daemon-reload
    
    # Enable and start service
    sudo systemctl enable passflow
    
    echo ""
    echo "Service installed successfully!"
    echo ""
    echo "To start the service:"
    echo "  sudo systemctl start passflow"
    echo ""
    echo "To check service status:"
    echo "  sudo systemctl status passflow"
    echo ""
    echo "To view logs:"
    echo "  sudo journalctl -u passflow -f"
    echo ""
fi

echo ""
echo "================================"
echo "Installation completed!"
echo "================================"
echo ""
echo "Directory structure created in ~/PassFlow/"
echo "Executable: build/passflow"
echo ""
echo "Next steps:"
echo "1. Edit ~/PassFlow/config.ini with your camera settings"
echo "2. Connect your CH340 USB-Serial device"
echo "3. Ensure your IP cameras are accessible"
echo "4. Run: ./build/passflow"
echo ""

if [ "$1" != "--service" ]; then
    echo "To install as a system service, run:"
    echo "  sudo ./install.sh --service"
    echo ""
fi
