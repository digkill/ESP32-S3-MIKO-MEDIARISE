# Endpoints Configuration

This document explains how to configure endpoints using the `.env` file.

## Overview

All endpoints used by the Xiaozhi ESP32 device can be configured through a `.env` file. The configuration is loaded at build time and used as default values if not set in device settings (NVS).

## Setup

1. Copy the example file to create your `.env` file:
   ```bash
   cp endpoints.env.example .env
   ```

2. Edit `.env` and fill in your endpoint values:
   ```bash
   # OTA (Over-The-Air) Update Endpoint
   OTA_URL=http://212.67.8.211:8080/ota/
   
   # WebSocket Server Configuration
   WEBSOCKET_URL=wss://your-server.com/ws
   WEBSOCKET_TOKEN=your_token_here
   WEBSOCKET_VERSION=3
   
   # MQTT Server Configuration
   MQTT_ENDPOINT=mqtt.example.com:8883
   MQTT_CLIENT_ID=your_client_id
   MQTT_USERNAME=your_username
   MQTT_PASSWORD=your_password
   MQTT_PUBLISH_TOPIC=device/commands
   MQTT_KEEPALIVE=240
   
   # Audio Debug UDP Server (for development/debugging)
   AUDIO_DEBUG_UDP_SERVER=192.168.2.100:8000
   ```

3. Generate the configuration header:
   ```bash
   python3 scripts/load_env_to_settings.py --generate-header
   ```

   This will generate `main/endpoints_config.h` with default values from your `.env` file.

4. Rebuild the project:
   ```bash
   idf.py build
   ```

## How It Works

1. **Build Time**: The `.env` file is read by `scripts/load_env_to_settings.py` and generates `main/endpoints_config.h` with C preprocessor defines.

2. **Runtime Priority**:
   - First, the code checks device settings (NVS) for endpoint values
   - If not found in settings, it falls back to values from `endpoints_config.h`
   - If not in the header, it uses Kconfig defaults (for OTA URL and Audio Debug UDP)

3. **Settings Override**: Endpoint values can be updated at runtime through:
   - OTA endpoint response (for WebSocket and MQTT configuration)
   - Manual settings update via serial/network interface

## Endpoints

### OTA URL
- **Setting Key**: `wifi.ota_url`
- **Default**: From `CONFIG_OTA_URL` (Kconfig) or `DEFAULT_OTA_URL` (.env)
- **Usage**: Used for firmware update checks and device activation

### WebSocket Configuration
- **Setting Keys**: 
  - `websocket.url` - WebSocket server URL
  - `websocket.token` - Authentication token
  - `websocket.version` - Protocol version (1, 2, or 3)
- **Defaults**: From `.env` file (`DEFAULT_WEBSOCKET_URL`, `DEFAULT_WEBSOCKET_TOKEN`, `DEFAULT_WEBSOCKET_VERSION`)

### MQTT Configuration
- **Setting Keys**:
  - `mqtt.endpoint` - MQTT broker address (format: `host:port`)
  - `mqtt.client_id` - MQTT client ID
  - `mqtt.username` - MQTT username
  - `mqtt.password` - MQTT password
  - `mqtt.publish_topic` - Topic for publishing messages
  - `mqtt.keepalive` - Keepalive interval in seconds
- **Defaults**: From `.env` file (`DEFAULT_MQTT_*`)

### Audio Debug UDP Server
- **Config**: `CONFIG_AUDIO_DEBUG_UDP_SERVER` (Kconfig) or `DEFAULT_AUDIO_DEBUG_UDP_SERVER` (.env)
- **Usage**: For audio debugging during development
- **Format**: `IP:PORT` (e.g., `192.168.2.100:8000`)

## Files

- `endpoints.env.example` - Template file with all available endpoints
- `.env` - Your local configuration (not tracked in git)
- `scripts/load_env_to_settings.py` - Script to generate config header from .env
- `main/endpoints_config.h` - Generated header file (auto-generated, do not edit manually)

## Notes

- The `.env` file is in `.gitignore` and will not be committed to the repository
- Always use `endpoints.env.example` as a template
- Regenerate `endpoints_config.h` after modifying `.env`
- Empty values in `.env` will not override existing settings





