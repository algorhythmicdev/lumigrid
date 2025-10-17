#pragma once
#include "driver/gpio.h"

// Addressable (RMT) GPIOs for ALEDch1..8
static const gpio_num_t ALED_GPIO[8] = {
  GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_17, GPIO_NUM_18,
  GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_26, GPIO_NUM_27
};

// I2C
#define I2C_SDA GPIO_NUM_21
#define I2C_SCL GPIO_NUM_22
#define PCA9685_I2C_ADDR 0x40

// PCA9685 OE (active-low)
#define PCA9685_OE GPIO_NUM_25

// UART service
#define UART_TX GPIO_NUM_1
#define UART_RX GPIO_NUM_3

// Boot
#define BOOT_IO GPIO_NUM_0

// Logical PWM mapping: LEDch1..8 -> PCA9685 channels 0..7
static const uint8_t LEDCH_TO_PCA[8] = { 0,1,2,3,4,5,6,7 };
