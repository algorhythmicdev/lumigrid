# Development Progress Log

## [2024-07-06]
- Scaffolded ESP-IDF project layout and copied hardware configuration defaults.
- Implemented PCA9685 driver (`components/pca9685_driver`) with init, duty, fade, and all-off support, wiring bootstrap in `app_main.c`.
- Added Unity-based unit test (`main/tests/test_pca9685_driver.c`) that verifies channel toggling for outputs 0–7 using an I²C stub.
- Documented build and setup steps in `README.md` and introduced `Kconfig.projbuild` option for enabling internal tests.
