/*
 * Copyright (C)2019 Kai Ludwig, DG4KLU
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <gpio.h>

void LEDsInit(void)
{
	gpioInitLEDs();

    GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
    GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 0);

#if defined(PLATFORM_RD5R)
	GPIO_PinWrite(GPIO_Torch, Pin_Torch, 0);
#endif
}

#if defined(PLATFORM_RD5R)
// Baofeng DM-5R torch LED
static bool torch_state = false;

void toggle_torch(void)
{
	torch_state = !torch_state;
	GPIO_PinWrite(GPIO_Torch, Pin_Torch, torch_state);
}
#endif
