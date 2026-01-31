#!/usr/bin/env python3
"""
PassFlow Serial Test Utility

Simulates peripheral device commands for testing the PassFlow system.
Sends commands via USB Serial port to test MainControl block.

Usage:
    python3 test_serial.py /dev/ttyUSB0
"""

import sys
import serial
import time
from typing import Optional

class PeripheralSimulator:
    """Simulates peripheral device commands"""
    
    # Command byte codes (matching ReceivedCommand enum)
    COMMANDS = {
        'Door0_Open': 0x01,
        'Door0_Close': 0x02,
        'Door1_Open': 0x03,
        'Door1_Close': 0x04,
        'MainSupplyON': 0x05,
        'MainSupplyOFF': 0x06,
        'IgnitionON': 0x07,
        'IgnitionOFF': 0x08,
        'Cover0Opened': 0x09,
        'Cover0Closed': 0x0A,
        'Cover1Opened': 0x0B,
        'Cover1Closed': 0x0C,
    }
    
    def __init__(self, port: str, baudrate: int = 115200):
        """Initialize serial connection"""
        try:
            self.serial = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1
            )
            print(f"Connected to {port} at {baudrate} baud")
        except serial.SerialException as e:
            print(f"Error opening serial port: {e}")
            sys.exit(1)
    
    def send_command(self, command: str) -> bool:
        """Send 1-byte command to PassFlow system"""
        if command not in self.COMMANDS:
            print(f"Error: Unknown command '{command}'")
            return False
        
        try:
            cmd_byte = self.COMMANDS[command]
            self.serial.write(bytes([cmd_byte]))
            print(f"Sent: {command} (0x{cmd_byte:02X})")
            return True
        except serial.SerialException as e:
            print(f"Error sending command: {e}")
            return False
    
    def receive_byte(self) -> Optional[int]:
        """Receive a byte from PassFlow system (LED/Camera/Light commands)"""
        try:
            if self.serial.in_waiting > 0:
                byte = self.serial.read(1)
                return ord(byte)
        except serial.SerialException as e:
            print(f"Error receiving: {e}")
        return None
    
    def close(self):
        """Close serial connection"""
        if self.serial.is_open:
            self.serial.close()
            print("Connection closed")

def print_menu():
    """Print command menu"""
    print("\n" + "="*50)
    print("PassFlow Serial Test Utility")
    print("="*50)
    print("Commands:")
    print("  1. Door0_Open")
    print("  2. Door0_Close")
    print("  3. Door1_Open")
    print("  4. Door1_Close")
    print("  5. MainSupplyON")
    print("  6. MainSupplyOFF")
    print("  7. IgnitionON")
    print("  8. IgnitionOFF")
    print("  9. Cover0Opened")
    print(" 10. Cover0Closed")
    print(" 11. Cover1Opened")
    print(" 12. Cover1Closed")
    print()
    print(" 20. Simulate Door0 Cycle (Open -> Wait 5s -> Close)")
    print(" 21. Simulate Door1 Cycle (Open -> Wait 5s -> Close)")
    print()
    print("  r. Monitor received bytes")
    print("  q. Quit")
    print("="*50)

def decode_received_byte(byte: int) -> str:
    """Decode received byte to command name"""
    commands = {
        0x10: "RedLedON",
        0x11: "RedLedOFF",
        0x12: "RedLedBlink",
        0x13: "GreenLedON",
        0x14: "GreenLedOFF",
        0x15: "GreenLedBlink",
        0x16: "BlueLedON",
        0x17: "BlueLedOFF",
        0x18: "BlueLedBlink",
        0x19: "Cam0ON",
        0x1A: "Cam0OFF",
        0x1B: "Cam1ON",
        0x1C: "Cam1OFF",
        0x1D: "Light0ON",
        0x1E: "Light0OFF",
        0x1F: "Light1ON",
        0x20: "Light1OFF",
        0x21: "FanON",
        0x22: "FanOFF",
    }
    return commands.get(byte, f"Unknown(0x{byte:02X})")

def monitor_received(simulator: PeripheralSimulator, duration: int = 10):
    """Monitor received bytes for specified duration"""
    print(f"\nMonitoring for {duration} seconds...")
    start_time = time.time()
    
    while time.time() - start_time < duration:
        byte = simulator.receive_byte()
        if byte is not None:
            cmd_name = decode_received_byte(byte)
            print(f"Received: 0x{byte:02X} - {cmd_name}")
        time.sleep(0.1)
    
    print("Monitoring stopped")

def simulate_door_cycle(simulator: PeripheralSimulator, door_id: int):
    """Simulate complete door open/close cycle"""
    print(f"\n--- Starting Door{door_id} Cycle Simulation ---")
    
    # Send door open
    open_cmd = f"Door{door_id}_Open"
    print(f"Step 1: Sending {open_cmd}")
    simulator.send_command(open_cmd)
    
    # Monitor for camera/light on commands
    print("Waiting for Cam/Light ON commands...")
    time.sleep(1)
    for _ in range(10):
        byte = simulator.receive_byte()
        if byte is not None:
            print(f"  Received: 0x{byte:02X} - {decode_received_byte(byte)}")
        time.sleep(0.1)
    
    # Wait to simulate door being open
    print("\nStep 2: Door open, waiting 5 seconds...")
    for i in range(5, 0, -1):
        print(f"  {i}...", end='\r')
        time.sleep(1)
    print()
    
    # Send door close
    close_cmd = f"Door{door_id}_Close"
    print(f"Step 3: Sending {close_cmd}")
    simulator.send_command(close_cmd)
    
    # Monitor for camera/light off commands
    print("Waiting for Cam/Light OFF commands...")
    time.sleep(1)
    for _ in range(10):
        byte = simulator.receive_byte()
        if byte is not None:
            print(f"  Received: 0x{byte:02X} - {decode_received_byte(byte)}")
        time.sleep(0.1)
    
    print("--- Door Cycle Complete ---\n")

def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("Usage: python3 test_serial.py <serial_port>")
        print("Example: python3 test_serial.py /dev/ttyUSB0")
        sys.exit(1)
    
    port = sys.argv[1]
    simulator = PeripheralSimulator(port)
    
    # Get command list for menu
    command_list = list(simulator.COMMANDS.keys())
    
    try:
        while True:
            print_menu()
            choice = input("Enter choice: ").strip()
            
            if choice == 'q':
                break
            elif choice == 'r':
                monitor_received(simulator, 10)
            elif choice == '20':
                simulate_door_cycle(simulator, 0)
            elif choice == '21':
                simulate_door_cycle(simulator, 1)
            elif choice.isdigit():
                idx = int(choice) - 1
                if 0 <= idx < len(command_list):
                    simulator.send_command(command_list[idx])
                else:
                    print("Invalid choice")
            else:
                print("Invalid choice")
            
            # Check for any received bytes
            time.sleep(0.5)
            byte = simulator.receive_byte()
            if byte is not None:
                print(f"Received: 0x{byte:02X} - {decode_received_byte(byte)}")
    
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
    finally:
        simulator.close()

if __name__ == '__main__':
    main()
