## LumiGrid LED Node Firmware

This repository contains the ESP-IDF firmware for the LumiGrid LED node controller. The goal is to manage eight addressable LED channels and eight PWM channels while exposing REST, MQTT, and synchronization features.

### Repository Layout
- `firmware/`: ESP-IDF project skeleton ready for module implementations.
  - `main/`: Application entry point, task stubs, and configuration.
  - `components/`: Placeholder component directories matching the planned architecture.
  - `assets/webapp/`: Reserved for pre-compressed UI assets to mount in LittleFS.
- `Docs/`: Design references including architecture, API specification, and effect implementations.

### Development Setup
1. Install ESP-IDF v5.x and export the toolchain environment (`. $IDF_PATH/export.sh`).
2. Enter the `firmware/` directory and run `idf.py set-target esp32` once (only if not already set).
3. Build the project with `idf.py build`. The current skeleton is structured to compile and log a bootstrap message once ESP-IDF dependencies are in place.
4. Flash to a development board using `idf.py -p <PORT> flash monitor`.

### Next Steps
- Implement the PCA9685 driver (`components/pca9685_driver`) following the guidelines in `Docs/AGENT_TASKS.md`.
- Flesh out the Effect Engine, REST API, sync protocol, and other subsystems using the provided design documents.
- Populate `assets/webapp/` with the gzipped UI bundle and configure LittleFS.
- Enable `LumiGrid Firmware Options → Enable LumiGrid internal unit tests` in `idf.py menuconfig` to compile the PCA9685 Unity test when running `idf.py test`.
