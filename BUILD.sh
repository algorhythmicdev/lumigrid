#!/bin/bash

echo "=== LumiGrid Firmware Build Script ==="
echo ""

# Check if we're in the right directory
if [ ! -f "firmware/CMakeLists.txt" ]; then
    echo "Error: Must run from project root directory"
    echo "Current directory: $(pwd)"
    exit 1
fi

cd firmware

echo "Building LumiGrid firmware..."
echo "Directory: $(pwd)"
echo ""

# Run the build
idf.py build

BUILD_STATUS=$?

if [ $BUILD_STATUS -eq 0 ]; then
    echo ""
    echo "==================================="
    echo "✅ BUILD SUCCESSFUL"
    echo "==================================="
    echo ""
    echo "Next steps:"
    echo "  1. Flash: idf.py -p /dev/ttyUSB0 flash"
    echo "  2. Monitor: idf.py -p /dev/ttyUSB0 monitor"
    echo "  3. Or both: idf.py -p /dev/ttyUSB0 flash monitor"
    echo ""
    echo "Web UI will be available at:"
    echo "  - AP mode: http://192.168.4.1"
    echo "  - STA mode: http://<device-ip>"
    echo ""
else
    echo ""
    echo "==================================="
    echo "❌ BUILD FAILED"
    echo "==================================="
    echo ""
    echo "Common issues:"
    echo "  1. ESP-IDF not set up: source ~/esp/esp-idf/export.sh"
    echo "  2. Missing dependencies: pip install -r \$IDF_PATH/requirements.txt"
    echo "  3. Wrong directory: cd firmware/"
    echo ""
    exit 1
fi
