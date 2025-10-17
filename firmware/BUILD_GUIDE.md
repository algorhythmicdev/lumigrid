# LumiGrid Firmware - Build Guide

## Quick Start

```bash
# Navigate to firmware directory
cd /home/slaff/Documents/lumigrid-1/firmware

# Build the project
idf.py build

# Flash to ESP32 (replace /dev/ttyUSB0 with your port)
idf.py -p /dev/ttyUSB0 flash monitor
```

## Prerequisites

1. **ESP-IDF v5.x** installed and configured
   ```bash
   # Verify ESP-IDF is set up
   idf.py --version
   ```

2. **Python dependencies**
   ```bash
   python -m pip install -r $IDF_PATH/requirements.txt
   ```

## Project Structure

```
firmware/
├── CMakeLists.txt          # Root CMake file
├── sdkconfig.defaults      # Default configuration
├── main/                   # Main application
├── components/             # Custom components (8 modules)
└── assets/                 # Web UI files
```

## Build from Scratch

### Step 1: Configure Project (First Time Only)

```bash
cd firmware
idf.py menuconfig
```

**Important Settings:**
- Navigate to: `Component config → ESP32-specific → CPU frequency`
  - Set to **240 MHz**
- Navigate to: `Component config → FreeRTOS → Tick rate (Hz)`
  - Already set to **1000** in sdkconfig.defaults
- Navigate to: `Partition Table`
  - Select "Custom partition table CSV" (optional, for LittleFS)

### Step 2: Build

```bash
idf.py build
```

**Expected Output:**
```
Project build complete. To flash, run:
 idf.py flash
or
 idf.py -p (PORT) flash
```

### Step 3: Flash Firmware

```bash
# Auto-detect port
idf.py flash

# Or specify port manually
idf.py -p /dev/ttyUSB0 flash
```

### Step 4: Monitor Serial Output

```bash
idf.py monitor

# Or combined flash + monitor
idf.py flash monitor
```

**Exit monitor:** Press `Ctrl+]`

## Build Targets

```bash
# Clean build
idf.py fullclean
idf.py build

# Build only (no flash)
idf.py build

# Flash only (no build)
idf.py flash

# Erase flash completely
idf.py erase-flash

# Show size breakdown
idf.py size
idf.py size-components

# Generate compile_commands.json
idf.py reconfigure
```

## Troubleshooting

### Error: "CMake Error: The source directory does not appear to contain CMakeLists.txt"

**Solution:** Make sure you're in the `firmware/` directory:
```bash
cd /home/slaff/Documents/lumigrid-1/firmware
idf.py build
```

### Error: "Missing component 'esp_littlefs'"

**Solution:** Add ESP-IDF component manager dependency:
```bash
idf.py add-dependency "joltwallet/littlefs^1.10.2"
```

Or manually create `firmware/components/esp_littlefs/` and implement it.

### Error: "undefined reference to..."

**Solution:** Check that all components are listed in `main/CMakeLists.txt` REQUIRES section.

### Build is slow

**Solution:** Enable ccache:
```bash
export IDF_CCACHE_ENABLE=1
idf.py build
```

## Creating LittleFS Image

If you want to pre-populate the `/spiffs` filesystem:

```bash
# Install mkspiffs tool
# (See ESP-IDF documentation for installation)

# Create filesystem image
mkspiffs -c assets/webapp -b 4096 -p 256 -s 0x80000 spiffs.bin

# Flash filesystem partition
esptool.py --port /dev/ttyUSB0 write_flash 0x110000 spiffs.bin
```

## Partition Table

Default partition layout (if using custom):

```
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x100000,
storage,  data, littlefs,0x110000,0x80000,
```

Save as `partitions.csv` and configure in menuconfig.

## Common Build Commands

```bash
# Full rebuild
idf.py fullclean build flash monitor

# Build + flash + monitor (typical workflow)
idf.py build flash monitor

# Build for release (optimized)
idf.py set-target esp32
idf.py -DCMAKE_BUILD_TYPE=Release build

# Generate documentation
idf.py docs

# Run menuconfig
idf.py menuconfig
```

## Environment Variables

```bash
# Set serial port (optional)
export ESPPORT=/dev/ttyUSB0

# Set baud rate
export ESPBAUD=921600

# Enable ccache
export IDF_CCACHE_ENABLE=1

# Set Python interpreter
export ESP_PYTHON=python3
```

## Debugging

### Enable verbose build output
```bash
idf.py -v build
```

### Enable debug logs in firmware
Edit `sdkconfig.defaults` or use menuconfig:
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y
```

### Monitor with filtering
```bash
# Show only errors and warnings
idf.py monitor --print_filter="*:E"

# Show specific tag
idf.py monitor --print_filter="REST_API:I"
```

## Build Performance

Typical build times:
- **Clean build:** 30-60 seconds (depending on CPU)
- **Incremental build:** 5-10 seconds
- **With ccache (subsequent):** 3-5 seconds

Reduce build time:
```bash
# Use all CPU cores
idf.py -j$(nproc) build

# Enable ccache
export IDF_CCACHE_ENABLE=1
```

## Next Steps After Successful Build

1. **Connect to Wi-Fi:**
   - On first boot, device creates AP "LumiGrid-Setup"
   - Connect and configure via web UI

2. **Access Web UI:**
   - Open browser to `http://192.168.4.1` (AP mode)
   - Or device IP in STA mode

3. **Test REST API:**
   ```bash
   curl http://192.168.4.1/api/status
   ```

4. **Monitor SSE events:**
   ```bash
   curl -N http://192.168.4.1/events
   ```

## Continuous Integration

Example GitHub Actions workflow:
```yaml
name: Build Firmware
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.1
          target: esp32
          path: firmware
```

## Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html)
- [Project README](../README.md)
- [Architecture Documentation](../Docs/ARCHITECTURE.md)

---

**Last Updated:** 2025-10-17  
**ESP-IDF Version:** v5.x  
**Target:** ESP32-WROOM-32U
