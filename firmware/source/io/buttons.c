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

#include <buttons.h>
#include <pit.h>
#include <settings.h>
#include <usb_com.h>
#include <gpio.h>


static uint32_t prevButtonState;
volatile bool PTTLocked = false;

#define MBUTTON_PRESSED  0x01
#define MBUTTON_LONG     0x02

typedef enum
{
	MBUTTON_ORANGE,
	MBUTTON_SK1,
	MBUTTON_SK2,
	MBUTTON_MAX
} MBUTTON_t;

static uint8_t mbuttons;

void buttonsInit(void)
{
	gpioInitButtons();
    mbuttons = 0x00;
    prevButtonState = 0;
}

static bool isMButtonPressed(MBUTTON_t mbutton)
{
     return (((mbuttons >> (mbutton * 2)) & MBUTTON_PRESSED) & MBUTTON_PRESSED);
}

static bool isMButtonLong(MBUTTON_t mbutton)
{
     return (((mbuttons >> (mbutton * 2)) & MBUTTON_LONG) & MBUTTON_LONG);
}

static void checkMButtonState(MBUTTON_t mbutton)
{
	if (isMButtonPressed(mbutton) == false)
	{
		taskENTER_CRITICAL();
		timer_mbuttons[mbutton] = (nonVolatileSettings.keypadTimerLong * 1000);
		taskEXIT_CRITICAL();

		mbuttons |= (MBUTTON_PRESSED << (mbutton * 2));
		mbuttons &= ~(MBUTTON_LONG << (mbutton * 2));
	}
}

uint32_t buttonsRead(void)
{
	uint32_t result = BUTTON_NONE;

#if ! defined(PLATFORM_RD5R)
	if (GPIO_PinRead(GPIO_Orange, Pin_Orange) == 0)
	{
		result |= BUTTON_ORANGE;
		checkMButtonState(MBUTTON_ORANGE);
	}
#endif // ! PLATFORM_RD5R

	if (GPIO_PinRead(GPIO_PTT, Pin_PTT) == 0)
	{
		result |= BUTTON_PTT;
	}

	if (GPIO_PinRead(GPIO_SK1, Pin_SK1) == 0)
	{
		result |= BUTTON_SK1;
		checkMButtonState(MBUTTON_SK1);
	}

	if (GPIO_PinRead(GPIO_SK2, Pin_SK2) == 0)
	{
		result |= BUTTON_SK2;
		checkMButtonState(MBUTTON_SK2);
	}

	return result;
}

static void checkMButtons(uint32_t *buttons, MBUTTON_t mbutton, uint32_t buttonID, uint32_t buttonShortUp, uint32_t buttonLong)
{
	taskENTER_CRITICAL();
	uint32_t tmp_timer_mbutton = timer_mbuttons[mbutton];
	taskEXIT_CRITICAL();

	// Note: Short press are send async

	if ((*buttons & buttonID) && isMButtonPressed(mbutton) && isMButtonLong(mbutton))
	{
		// button is still down
		*buttons |= buttonLong;
	}
	else if ((*buttons & buttonID) && isMButtonPressed(mbutton) && (isMButtonLong(mbutton) == false))
	{
		if (tmp_timer_mbutton == 0)
		{
			// Long press
			mbuttons |= (MBUTTON_LONG << (mbutton * 2));

			// Set LONG bit
			*buttons |= buttonLong;
		}
	}
	else if (((*buttons & buttonID) == 0) && isMButtonPressed(mbutton) && (isMButtonLong(mbutton) == false) && (tmp_timer_mbutton != 0))
	{
		// Short press/release cycle
		mbuttons &= ~(MBUTTON_PRESSED << (mbutton * 2));
		mbuttons &= ~(MBUTTON_LONG << (mbutton * 2));

		taskENTER_CRITICAL();
		timer_mbuttons[mbutton] = 0;
		taskEXIT_CRITICAL();

		// Set SHORT press
		*buttons |= buttonShortUp;
		*buttons &= ~buttonLong;
	}
	else if (((*buttons & buttonID) == 0) && isMButtonPressed(mbutton) && isMButtonLong(mbutton))
	{
		// Button was still down after a long press, now handle release
		mbuttons &= ~(MBUTTON_PRESSED << (mbutton * 2));
		mbuttons &= ~(MBUTTON_LONG << (mbutton * 2));

		// Remove LONG
		*buttons &= ~buttonLong;
	}
}

void buttonsCheckButtonsEvent(uint32_t *buttons, int *event, bool keyIsDown)
{
	*buttons = buttonsRead();

#if ! defined(PLATFORM_RD5R)
	checkMButtons(buttons, MBUTTON_ORANGE, BUTTON_ORANGE, BUTTON_ORANGE_SHORT_UP, BUTTON_ORANGE_LONG_DOWN);
#endif // ! PLATFORM_RD5R
	checkMButtons(buttons, MBUTTON_SK1, BUTTON_SK1, BUTTON_SK1_SHORT_UP, BUTTON_SK1_LONG_DOWN);
	checkMButtons(buttons, MBUTTON_SK2, BUTTON_SK2, BUTTON_SK2_SHORT_UP, BUTTON_SK2_LONG_DOWN);

	if (prevButtonState != *buttons)
	{
		// If a keypad key is down, the buttons are acting as modifier and mask shortup/longdown status
		if (keyIsDown &&
				( (*buttons & BUTTON_SK1_SHORT_UP) || (*buttons & BUTTON_SK1_LONG_DOWN)
						|| (*buttons & BUTTON_SK2_SHORT_UP) || (*buttons & BUTTON_SK2_LONG_DOWN)
#if ! defined(PLATFORM_RD5R)
						|| (*buttons & BUTTON_ORANGE_SHORT_UP) || (*buttons & BUTTON_ORANGE_LONG_DOWN)
#endif // ! PLATFORM_RD5R
				) )
		{
			// Clear shortup/longdown bits
			*buttons &= ~(BUTTON_SK1_SHORT_UP | BUTTON_SK1_LONG_DOWN | BUTTON_SK2_SHORT_UP | BUTTON_SK2_LONG_DOWN
#if ! defined(PLATFORM_RD5R)
					| BUTTON_ORANGE_SHORT_UP | BUTTON_ORANGE_LONG_DOWN
#endif // ! PLATFORM_RD5R
			);

			if (prevButtonState != *buttons)
			{
				prevButtonState = *buttons;
				*event = EVENT_BUTTON_CHANGE;
			}
			else
			{
				*event = EVENT_BUTTON_NONE;
			}
		}
		else
		{
			prevButtonState = *buttons;
			*event = EVENT_BUTTON_CHANGE;
		}
	}
	else
	{
		*event = EVENT_BUTTON_NONE;
	}
}
