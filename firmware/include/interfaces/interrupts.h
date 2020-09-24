/*
 * Copyright (C)2020 Roger Clark VK3KYY
 *
 */
#ifndef _FW_INTERRUPTS_H_
#define _FW_INTERRUPTS_H_
#include <gpio.h>
#include <stdint.h>


void interruptsInitC6000Interface(void);
bool interruptsWasPinTriggered(PORT_Type *port, uint32_t pin);
bool interruptsClearPinFlags(PORT_Type *port, uint32_t pin);
#endif
