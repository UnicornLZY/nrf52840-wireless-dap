#pragma once
#include "nrf_base.h"
struct MockPower{uint32_t USBREGSTATUS=0;};
extern MockPower* NRF_POWER;
#define POWER_USBREGSTATUS_VBUSDETECT_Msk 1

extern MockGpio mockP1;
#define NRF_P1 (&mockP1)
#define GPIO_PIN_CNF_PULL_Msk 12
#define GPIO_PIN_CNF_PULL_Pullup 3
#define GPIO_PIN_CNF_PULL_Pos 2
