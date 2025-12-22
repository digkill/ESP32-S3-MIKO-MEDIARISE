#!/usr/bin/env python3
"""
Load .env file and update ESP32 device settings via serial port.
This script reads the .env file and sends commands to update NVS settings.
"""

import os
import sys
import serial
import serial.tools.list_ports
import time
import re
from pathlib import Path

# Add project root to path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))

def load_env_file(env_path):
    """Load environment variables from .env file"""
    env_vars = {}
    if not os.path.exists(env_path):
        print(f"Warning: .env file not found at {env_path}")
        return env_vars
    
    with open(env_path, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            # Parse KEY=VALUE
            if '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                # Remove quotes if present
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                elif value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                env_vars[key] = value
    return env_vars

def find_esp32_port():
    """Find ESP32 serial port"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Common ESP32 board identifiers
        if any(keyword in port.description.lower() for keyword in ['esp32', 'ch340', 'cp210', 'ftdi', 'usb serial']):
            return port.device
        if any(keyword in port.manufacturer.lower() for keyword in ['espressif', 'silicon labs', 'wch']):
            return port.device
    return None

def send_at_command(ser, command, timeout=5):
    """Send AT-like command and wait for response"""
    ser.write((command + '\r\n').encode())
    time.sleep(0.1)
    start_time = time.time()
    response = b''
    while time.time() - start_time < timeout:
        if ser.in_waiting:
            response += ser.read(ser.in_waiting)
            if b'OK' in response or b'ERROR' in response:
                break
        time.sleep(0.1)
    return response.decode('utf-8', errors='ignore')

def update_settings_via_serial(env_vars, port=None, baudrate=115200):
    """Update ESP32 settings via serial port"""
    if port is None:
        port = find_esp32_port()
        if port is None:
            print("Error: Could not find ESP32 serial port")
            print("Please specify port manually: python load_env_to_settings.py --port COM3")
            return False
    
    print(f"Connecting to {port} at {baudrate} baud...")
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2)  # Wait for device to be ready
        
        # Clear any existing data
        ser.reset_input_buffer()
        
        updated_count = 0
        
        # Update OTA URL
        if 'OTA_URL' in env_vars and env_vars['OTA_URL']:
            print(f"Setting OTA_URL: {env_vars['OTA_URL']}")
            # Note: This requires a custom command handler on the device
            # For now, we'll just print what needs to be set
            updated_count += 1
        
        # Update WebSocket settings
        if 'WEBSOCKET_URL' in env_vars and env_vars['WEBSOCKET_URL']:
            print(f"Setting WEBSOCKET_URL: {env_vars['WEBSOCKET_URL']}")
            updated_count += 1
        if 'WEBSOCKET_TOKEN' in env_vars and env_vars['WEBSOCKET_TOKEN']:
            print(f"Setting WEBSOCKET_TOKEN: {env_vars['WEBSOCKET_TOKEN']}")
            updated_count += 1
        if 'WEBSOCKET_VERSION' in env_vars and env_vars['WEBSOCKET_VERSION']:
            print(f"Setting WEBSOCKET_VERSION: {env_vars['WEBSOCKET_VERSION']}")
            updated_count += 1
        
        # Update MQTT settings
        if 'MQTT_ENDPOINT' in env_vars and env_vars['MQTT_ENDPOINT']:
            print(f"Setting MQTT_ENDPOINT: {env_vars['MQTT_ENDPOINT']}")
            updated_count += 1
        if 'MQTT_CLIENT_ID' in env_vars and env_vars['MQTT_CLIENT_ID']:
            print(f"Setting MQTT_CLIENT_ID: {env_vars['MQTT_CLIENT_ID']}")
            updated_count += 1
        if 'MQTT_USERNAME' in env_vars and env_vars['MQTT_USERNAME']:
            print(f"Setting MQTT_USERNAME: {env_vars['MQTT_USERNAME']}")
            updated_count += 1
        if 'MQTT_PASSWORD' in env_vars and env_vars['MQTT_PASSWORD']:
            print(f"Setting MQTT_PASSWORD: {env_vars['MQTT_PASSWORD']}")
            updated_count += 1
        if 'MQTT_PUBLISH_TOPIC' in env_vars and env_vars['MQTT_PUBLISH_TOPIC']:
            print(f"Setting MQTT_PUBLISH_TOPIC: {env_vars['MQTT_PUBLISH_TOPIC']}")
            updated_count += 1
        if 'MQTT_KEEPALIVE' in env_vars and env_vars['MQTT_KEEPALIVE']:
            print(f"Setting MQTT_KEEPALIVE: {env_vars['MQTT_KEEPALIVE']}")
            updated_count += 1
        
        ser.close()
        print(f"\nSettings update complete. {updated_count} settings would be updated.")
        print("Note: This script requires device-side support for setting values via serial.")
        print("For now, settings should be configured through the OTA endpoint response or manually.")
        return True
        
    except serial.SerialException as e:
        print(f"Error: Could not open serial port: {e}")
        return False

def generate_config_header(env_vars, output_path):
    """Generate a C++ header file with default values from .env"""
    header_content = """#ifndef ENDPOINTS_CONFIG_H
#define ENDPOINTS_CONFIG_H

// Auto-generated from .env file
// Do not edit manually - regenerate using: python scripts/load_env_to_settings.py --generate-header

"""
    
    if 'OTA_URL' in env_vars and env_vars['OTA_URL']:
        header_content += f'#define DEFAULT_OTA_URL "{env_vars["OTA_URL"]}"\n'
    
    if 'WEBSOCKET_URL' in env_vars and env_vars['WEBSOCKET_URL']:
        header_content += f'#define DEFAULT_WEBSOCKET_URL "{env_vars["WEBSOCKET_URL"]}"\n'
    
    if 'WEBSOCKET_TOKEN' in env_vars and env_vars['WEBSOCKET_TOKEN']:
        header_content += f'#define DEFAULT_WEBSOCKET_TOKEN "{env_vars["WEBSOCKET_TOKEN"]}"\n'
    
    if 'WEBSOCKET_VERSION' in env_vars and env_vars['WEBSOCKET_VERSION']:
        header_content += f'#define DEFAULT_WEBSOCKET_VERSION {env_vars["WEBSOCKET_VERSION"]}\n'
    
    if 'MQTT_ENDPOINT' in env_vars and env_vars['MQTT_ENDPOINT']:
        header_content += f'#define DEFAULT_MQTT_ENDPOINT "{env_vars["MQTT_ENDPOINT"]}"\n'
    
    if 'MQTT_CLIENT_ID' in env_vars and env_vars['MQTT_CLIENT_ID']:
        header_content += f'#define DEFAULT_MQTT_CLIENT_ID "{env_vars["MQTT_CLIENT_ID"]}"\n'
    
    if 'MQTT_USERNAME' in env_vars and env_vars['MQTT_USERNAME']:
        header_content += f'#define DEFAULT_MQTT_USERNAME "{env_vars["MQTT_USERNAME"]}"\n'
    
    if 'MQTT_PASSWORD' in env_vars and env_vars['MQTT_PASSWORD']:
        header_content += f'#define DEFAULT_MQTT_PASSWORD "{env_vars["MQTT_PASSWORD"]}"\n'
    
    if 'MQTT_PUBLISH_TOPIC' in env_vars and env_vars['MQTT_PUBLISH_TOPIC']:
        header_content += f'#define DEFAULT_MQTT_PUBLISH_TOPIC "{env_vars["MQTT_PUBLISH_TOPIC"]}"\n'
    
    if 'MQTT_KEEPALIVE' in env_vars and env_vars['MQTT_KEEPALIVE']:
        header_content += f'#define DEFAULT_MQTT_KEEPALIVE {env_vars["MQTT_KEEPALIVE"]}\n'
    
    if 'AUDIO_DEBUG_UDP_SERVER' in env_vars and env_vars['AUDIO_DEBUG_UDP_SERVER']:
        header_content += f'#define DEFAULT_AUDIO_DEBUG_UDP_SERVER "{env_vars["AUDIO_DEBUG_UDP_SERVER"]}"\n'
    
    header_content += "\n#endif // ENDPOINTS_CONFIG_H\n"
    
    with open(output_path, 'w') as f:
        f.write(header_content)
    
    print(f"Generated config header: {output_path}")

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Load .env file and update ESP32 settings')
    parser.add_argument('--env', default='.env', help='Path to .env file (default: .env)')
    parser.add_argument('--port', help='Serial port (auto-detect if not specified)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Serial baudrate (default: 115200)')
    parser.add_argument('--generate-header', action='store_true', 
                       help='Generate C++ header file instead of updating device')
    parser.add_argument('--output', default='main/endpoints_config.h',
                       help='Output path for generated header (default: main/endpoints_config.h)')
    
    args = parser.parse_args()
    
    env_path = project_root / args.env
    env_vars = load_env_file(env_path)
    
    if not env_vars:
        print("No environment variables found in .env file")
        return 1
    
    print(f"Loaded {len(env_vars)} environment variables from {env_path}")
    
    if args.generate_header:
        output_path = project_root / args.output
        generate_config_header(env_vars, output_path)
    else:
        update_settings_via_serial(env_vars, args.port, args.baudrate)
    
    return 0

if __name__ == '__main__':
    sys.exit(main())






