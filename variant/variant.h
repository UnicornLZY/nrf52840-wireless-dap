#pragma once
#include "WVariant.h"
#define VARIANT_MCK 64000000ul
#define USE_LFXO
// Keep RAW GPIO numbering, matching the user's working PCA10056 firmware.
// Physical D2 is GPIO 17, not Arduino digitalWrite(2).
#define PINS_COUNT 48
#define NUM_DIGITAL_PINS 48
#define NUM_ANALOG_INPUTS 6
#define NUM_ANALOG_OUTPUTS 0
#define PIN_LED1 15
#define LED_BUILTIN 15
#define LED_CONN 15
#define LED_BLUE 15
#define LED_RED 15
#define LED_STATE_ON 1
#define PIN_SERIAL1_RX 33
#define PIN_SERIAL1_TX 34
#define PIN_SERIAL2_RX 8
#define PIN_SERIAL2_TX 6
#define PIN_A0 3
#define PIN_A1 4
#define PIN_A2 28
#define PIN_A3 29
#define PIN_A4 30
#define PIN_A5 31
#define ADC_RESOLUTION 14
static const uint8_t A0=3,A1=4,A2=28,A3=29,A4=30,A5=31;
static const uint8_t AREF=2;
#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO 43
#define PIN_SPI_MOSI 10
#define PIN_SPI_SCK 45
static const uint8_t SS=9,MOSI=10,MISO=43,SCK=45;
#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA 17
#define PIN_WIRE_SCL 20
// No QSPI or external Flash: those pins are used for target SWD.
