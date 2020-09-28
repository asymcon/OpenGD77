/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
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
#include <hardware/UC1701.h>
#include <settings.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiLocalisation.h>
#include <user_interface/uiUtilities.h>

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);
static void updateBacklightMode(uint8_t mode);
static void setDisplayInvert(bool invert);
static void checkMinBacklightValue(void);

static menuStatus_t menuDisplayOptionsExitCode = MENU_STATUS_SUCCESS;

static const int BACKLIGHT_MAX_TIMEOUT = 60;
#if defined (PLATFORM_RD5R)
	static const int CONTRAST_MAX_VALUE = 10;// Maximum value which still seems to be readable
	static const int CONTRAST_MIN_VALUE = 0;// Minimum value which still seems to be readable
#else
	static const int CONTRAST_MAX_VALUE = 30;// Maximum value which still seems to be readable
	static const int CONTRAST_MIN_VALUE = 5;// Minimum value which still seems to be readable
#endif

static const int BACKLIGHT_TIMEOUT_STEP = 1;
static const int BACKLIGHT_MAX_PERCENTAGE = 100;
static const int BACKLIGHT_PERCENTAGE_STEP = 10;
static const int BACKLIGHT_PERCENTAGE_STEP_SMALL = 1;

static const char *contactOrders[] = { "Ct/DB/TA", "DB/Ct/TA", "TA/Ct/DB", "TA/DB/Ct" };

enum DISPLAY_MENU_LIST { DISPLAY_MENU_BRIGHTNESS = 0, DISPLAY_MENU_BRIGHTNESS_OFF, DISPLAY_MENU_CONTRAST, DISPLAY_MENU_BACKLIGHT_MODE,
	DISPLAY_MENU_TIMEOUT, DISPLAY_MENU_COLOUR_INVERT, DISPLAY_MENU_CONTACT_DISPLAY_ORDER, DISPLAY_MENU_CONTACT_DISPLAY_SPLIT_CONTACT,
	NUM_DISPLAY_MENU_ITEMS };

menuStatus_t menuDisplayOptions(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		// Store original settings, used on cancel event.
		memcpy(&originalNonVolatileSettings, &nonVolatileSettings, sizeof(settingsStruct_t));

		voicePromptsInit();
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendLanguageString(&currentLanguage->display_options);
		voicePromptsAppendLanguageString(&currentLanguage->menu);
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendPrompt(PROMPT_SILENCE);

		updateScreen(true);
		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuDisplayOptionsExitCode = MENU_STATUS_SUCCESS;

		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return menuDisplayOptionsExitCode;
}

static void updateScreen(bool isFirstRun)
{
	int mNum = 0;
	static const int bufferLen = 17;
	char buf[bufferLen];
	char * const *leftSide = NULL;// initialize to please the compiler
	char * const *rightSideConst = NULL;// initialize to please the compiler
	char rightSideVar[bufferLen];

	ucClearBuf();
	menuDisplayTitle(currentLanguage->display_options);

	// Can only display 3 of the options at a time menu at -1, 0 and +1
	for(int i = -1; i <= 1; i++)
	{
		mNum = menuGetMenuOffset(NUM_DISPLAY_MENU_ITEMS, i);
		buf[0] = 0;
		rightSideVar[0] = 0;
		rightSideConst = NULL;

		switch(mNum)
		{
			case DISPLAY_MENU_BRIGHTNESS:
				leftSide = (char * const *)&currentLanguage->brightness;
				snprintf(rightSideVar, bufferLen, "%d%%", nonVolatileSettings.displayBacklightPercentage);
				break;
			case DISPLAY_MENU_BRIGHTNESS_OFF:
				leftSide = (char * const *)&currentLanguage->brightness_off;
				snprintf(rightSideVar, bufferLen, "%d%%", nonVolatileSettings.displayBacklightPercentageOff);
				break;
			case DISPLAY_MENU_CONTRAST:
				leftSide = (char * const *)&currentLanguage->contrast;
				snprintf(rightSideVar, bufferLen, "%d", nonVolatileSettings.displayContrast);
				break;
			case DISPLAY_MENU_BACKLIGHT_MODE:
				{
					const char * const *backlightModes[] = { &currentLanguage->Auto, &currentLanguage->squelch, &currentLanguage->manual, &currentLanguage->none };
					leftSide = (char * const *)&currentLanguage->mode;
					rightSideConst = (char * const *)backlightModes[nonVolatileSettings.backlightMode];
				}
				break;
			case DISPLAY_MENU_TIMEOUT:
				leftSide = (char * const *)&currentLanguage->backlight_timeout;
				if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_AUTO) || (nonVolatileSettings.backlightMode == BACKLIGHT_MODE_SQUELCH))
				{
					if (nonVolatileSettings.backLightTimeout == 0)
					{
						rightSideConst = (char * const *)&currentLanguage->off;
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "%ds", nonVolatileSettings.backLightTimeout);
					}
				}
				else
				{
					rightSideConst = (char * const *)&currentLanguage->n_a;
				}
				break;
			case DISPLAY_MENU_COLOUR_INVERT:
				leftSide = (char * const *)&currentLanguage->display_background_colour;
				rightSideConst = nonVolatileSettings.displayInverseVideo ? (char * const *)&currentLanguage->colour_invert : (char * const *)&currentLanguage->colour_normal;
				break;
			case DISPLAY_MENU_CONTACT_DISPLAY_ORDER:
				leftSide = (char * const *)&currentLanguage->priority_order;
				snprintf(rightSideVar, bufferLen, "%s",contactOrders[nonVolatileSettings.contactDisplayPriority]);
				break;
			case DISPLAY_MENU_CONTACT_DISPLAY_SPLIT_CONTACT:
				{
					const char * const *splitContact[] = { &currentLanguage->one_line, &currentLanguage->two_lines, &currentLanguage->Auto };
					leftSide = (char * const *)&currentLanguage->contact;
					rightSideConst = (char * const *)splitContact[nonVolatileSettings.splitContact];
				}
				break;
		}

		// workaround for non stardard format of line for colour display

		snprintf(buf, bufferLen, "%s:%s", *leftSide, (rightSideVar[0] ? rightSideVar : *rightSideConst));

		if (i == 0)
		{
			if (!isFirstRun)
			{
				voicePromptsInit();
			}
			voicePromptsAppendLanguageString((const char * const *)leftSide);

			if (rightSideVar[0] != 0)
			{
				voicePromptsAppendString(rightSideVar);
			}
			else
			{
				voicePromptsAppendLanguageString((const char * const *)rightSideConst);
			}
			voicePromptsPlay();
		}

		buf[bufferLen - 1] = 0;
		menuDisplayEntry(i, mNum, buf);
	}

	ucRender();
	displayLightTrigger();
}

static void handleEvent(uiEvent_t *ev)
{
	bool isDirty = false;
	displayLightTrigger();

	if (ev->events & FUNCTION_EVENT)
	{
		isDirty = true;
		if (ev->function == DEC_BRIGHTNESS)
		{
			settingsDecrement(nonVolatileSettings.displayBacklightPercentage,
					((nonVolatileSettings.displayBacklightPercentage <= BACKLIGHT_PERCENTAGE_STEP) ? BACKLIGHT_PERCENTAGE_STEP_SMALL : BACKLIGHT_PERCENTAGE_STEP));

			if (nonVolatileSettings.displayBacklightPercentage < 0)
			{
				settingsSet(nonVolatileSettings.displayBacklightPercentage, 0);
			}
			displayLightTrigger();
			menuSystemPopPreviousMenu();
			return;
			//			gMenusCurrentItemIndex = DISPLAY_MENU_BRIGHTNESS;
		}
		else if (ev->function == INC_BRIGHTNESS)
		{
			settingsIncrement(nonVolatileSettings.displayBacklightPercentage,
					((nonVolatileSettings.displayBacklightPercentage < BACKLIGHT_PERCENTAGE_STEP) ? BACKLIGHT_PERCENTAGE_STEP_SMALL : BACKLIGHT_PERCENTAGE_STEP));

			if (nonVolatileSettings.displayBacklightPercentage > BACKLIGHT_MAX_PERCENTAGE)
			{
				settingsSet(nonVolatileSettings.displayBacklightPercentage, BACKLIGHT_MAX_PERCENTAGE);
			}
			displayLightTrigger();
			menuSystemPopPreviousMenu();
			return;
		}
	}

	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	if (ev->events & KEY_EVENT)
	{
		bool displayIsLit = displayIsBacklightLit();

		if (KEYCHECK_PRESS(ev->keys,KEY_DOWN))
		{
			isDirty = true;
			menuSystemMenuIncrement(&gMenusCurrentItemIndex, NUM_DISPLAY_MENU_ITEMS);
			menuDisplayOptionsExitCode |= MENU_STATUS_LIST_TYPE;
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
		{
			isDirty = true;
			menuSystemMenuDecrement(&gMenusCurrentItemIndex, NUM_DISPLAY_MENU_ITEMS);
			menuDisplayOptionsExitCode |= MENU_STATUS_LIST_TYPE;
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_RIGHT))
		{
			isDirty = true;
			switch(gMenusCurrentItemIndex)
			{
				case DISPLAY_MENU_BRIGHTNESS:
					settingsIncrement(nonVolatileSettings.displayBacklightPercentage,
							((nonVolatileSettings.displayBacklightPercentage < BACKLIGHT_PERCENTAGE_STEP) ? BACKLIGHT_PERCENTAGE_STEP_SMALL : BACKLIGHT_PERCENTAGE_STEP));

					if (nonVolatileSettings.displayBacklightPercentage > BACKLIGHT_MAX_PERCENTAGE)
					{
						settingsSet(nonVolatileSettings.displayBacklightPercentage, BACKLIGHT_MAX_PERCENTAGE);
					}
					break;
				case DISPLAY_MENU_BRIGHTNESS_OFF:
					if (nonVolatileSettings.displayBacklightPercentageOff < nonVolatileSettings.displayBacklightPercentage)
					{
						settingsIncrement(nonVolatileSettings.displayBacklightPercentageOff,
								((nonVolatileSettings.displayBacklightPercentageOff < BACKLIGHT_PERCENTAGE_STEP) ? BACKLIGHT_PERCENTAGE_STEP_SMALL : BACKLIGHT_PERCENTAGE_STEP));

						if (nonVolatileSettings.displayBacklightPercentageOff > BACKLIGHT_MAX_PERCENTAGE)
						{
							settingsSet(nonVolatileSettings.displayBacklightPercentageOff, BACKLIGHT_MAX_PERCENTAGE);
						}

						checkMinBacklightValue();

						if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_MANUAL) && (!displayIsLit))
						{
							gpioSetDisplayBacklightIntensityPercentage(nonVolatileSettings.displayBacklightPercentageOff);
						}
					}
					break;
				case DISPLAY_MENU_CONTRAST:
					if (nonVolatileSettings.displayContrast < CONTRAST_MAX_VALUE)
					{
						settingsIncrement(nonVolatileSettings.displayContrast, 1);
					}
					ucSetContrast(nonVolatileSettings.displayContrast);
					break;
				case DISPLAY_MENU_BACKLIGHT_MODE:
					if (nonVolatileSettings.backlightMode < BACKLIGHT_MODE_NONE)
					{
						settingsIncrement(nonVolatileSettings.backlightMode, 1);
						updateBacklightMode(nonVolatileSettings.backlightMode);
					}
					break;
				case DISPLAY_MENU_TIMEOUT:
					if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_AUTO) || (nonVolatileSettings.backlightMode == BACKLIGHT_MODE_SQUELCH))
					{
						settingsIncrement(nonVolatileSettings.backLightTimeout, BACKLIGHT_TIMEOUT_STEP);
						if (nonVolatileSettings.backLightTimeout > BACKLIGHT_MAX_TIMEOUT)
						{
							settingsSet(nonVolatileSettings.backLightTimeout, BACKLIGHT_MAX_TIMEOUT);
						}
					}
					break;
				case DISPLAY_MENU_COLOUR_INVERT:
					setDisplayInvert(true);
					break;
				case DISPLAY_MENU_CONTACT_DISPLAY_ORDER:
					if (nonVolatileSettings.contactDisplayPriority < CONTACT_DISPLAY_PRIO_TA_DB_CC)
					{
						settingsIncrement(nonVolatileSettings.contactDisplayPriority, 1);
					}
					break;
				case DISPLAY_MENU_CONTACT_DISPLAY_SPLIT_CONTACT:
					if (nonVolatileSettings.splitContact < SPLIT_CONTACT_AUTO)
					{
						settingsIncrement(nonVolatileSettings.splitContact, 1);
					}
					break;
			}
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_LEFT))
		{
			isDirty = true;
			switch(gMenusCurrentItemIndex)
			{
				case DISPLAY_MENU_BRIGHTNESS:
					settingsDecrement(nonVolatileSettings.displayBacklightPercentage,
							((nonVolatileSettings.displayBacklightPercentage <= BACKLIGHT_PERCENTAGE_STEP) ? 1 : BACKLIGHT_PERCENTAGE_STEP));

					if (nonVolatileSettings.displayBacklightPercentage < 0)
					{
						settingsSet(nonVolatileSettings.displayBacklightPercentage, 0);
					}

					checkMinBacklightValue();
					break;
				case DISPLAY_MENU_BRIGHTNESS_OFF:
					settingsDecrement(nonVolatileSettings.displayBacklightPercentageOff,
							((nonVolatileSettings.displayBacklightPercentageOff <= BACKLIGHT_PERCENTAGE_STEP) ? BACKLIGHT_PERCENTAGE_STEP_SMALL : BACKLIGHT_PERCENTAGE_STEP));

					if (nonVolatileSettings.displayBacklightPercentageOff < 0)
					{
						settingsSet(nonVolatileSettings.displayBacklightPercentageOff, 0);
					}

					if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_MANUAL) && (!displayIsLit))
					{
						gpioSetDisplayBacklightIntensityPercentage(nonVolatileSettings.displayBacklightPercentageOff);
					}
					break;
				case DISPLAY_MENU_CONTRAST:
					if (nonVolatileSettings.displayContrast > CONTRAST_MIN_VALUE)
					{
						settingsDecrement(nonVolatileSettings.displayContrast, 1);
					}
					ucSetContrast(nonVolatileSettings.displayContrast);
					break;
				case DISPLAY_MENU_BACKLIGHT_MODE:
					if (nonVolatileSettings.backlightMode > BACKLIGHT_MODE_AUTO)
					{
						settingsDecrement(nonVolatileSettings.backlightMode, 1);
						updateBacklightMode(nonVolatileSettings.backlightMode);
					}
					break;
				case DISPLAY_MENU_TIMEOUT:
					if (((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_AUTO) && (nonVolatileSettings.backLightTimeout >= BACKLIGHT_TIMEOUT_STEP)) ||
							((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_SQUELCH) && (nonVolatileSettings.backLightTimeout >= (BACKLIGHT_TIMEOUT_STEP * 2))))
					{
						settingsDecrement(nonVolatileSettings.backLightTimeout, BACKLIGHT_TIMEOUT_STEP);
					}
					break;
				case DISPLAY_MENU_COLOUR_INVERT:
					setDisplayInvert(false);
					break;
				case DISPLAY_MENU_CONTACT_DISPLAY_ORDER:
					if (nonVolatileSettings.contactDisplayPriority > CONTACT_DISPLAY_PRIO_CC_DB_TA)
					{
						settingsDecrement(nonVolatileSettings.contactDisplayPriority, 1);
					}
					break;
				case DISPLAY_MENU_CONTACT_DISPLAY_SPLIT_CONTACT:
					if (nonVolatileSettings.splitContact > SPLIT_CONTACT_SINGLE_LINE_ONLY)
					{
						settingsDecrement(nonVolatileSettings.splitContact, 1);
					}
					break;
			}
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
		{
			// All parameters has already been applied
			settingsSaveIfNeeded(true);
			menuSystemPopAllAndDisplayRootMenu();
			return;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
		{
			bool displayIsLit = displayIsBacklightLit();

			if (nonVolatileSettings.displayContrast != originalNonVolatileSettings.displayContrast)
			{
				settingsSet(nonVolatileSettings.displayContrast, originalNonVolatileSettings.displayContrast);
				ucSetContrast(nonVolatileSettings.displayContrast);
			}

			if (nonVolatileSettings.displayInverseVideo != originalNonVolatileSettings.displayInverseVideo)
			{
				settingsSet(nonVolatileSettings.displayInverseVideo, originalNonVolatileSettings.displayInverseVideo);
				displayInit(nonVolatileSettings.displayInverseVideo);// Need to perform a full reset on the display to change back to non-inverted
			}

			settingsSet(nonVolatileSettings.displayBacklightPercentage, originalNonVolatileSettings.displayBacklightPercentage);
			settingsSet(nonVolatileSettings.displayBacklightPercentageOff, originalNonVolatileSettings.displayBacklightPercentageOff);
			settingsSet(nonVolatileSettings.backLightTimeout, originalNonVolatileSettings.backLightTimeout);

			if (nonVolatileSettings.backlightMode != originalNonVolatileSettings.backlightMode)
			{
				updateBacklightMode(originalNonVolatileSettings.backlightMode);
			}

			if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_MANUAL) && (!displayIsLit))
			{
				gpioSetDisplayBacklightIntensityPercentage(nonVolatileSettings.displayBacklightPercentageOff);
			}

			settingsSaveIfNeeded(true);
			menuSystemPopPreviousMenu();
			return;
		}
	}

	if (isDirty)
	{
		updateScreen(false);
	}
}

static void updateBacklightMode(uint8_t mode)
{
	settingsSet(nonVolatileSettings.backlightMode, mode);

	switch (mode)
	{
		case BACKLIGHT_MODE_MANUAL:
		case BACKLIGHT_MODE_NONE:
			displayEnableBacklight(false); // Could be MANUAL previously, but in OFF state, so turn it OFF blindly.
			break;
		case BACKLIGHT_MODE_SQUELCH:
			if (nonVolatileSettings.backLightTimeout < BACKLIGHT_TIMEOUT_STEP)
			{
				settingsSet(nonVolatileSettings.backLightTimeout, BACKLIGHT_TIMEOUT_STEP);
			}
		case BACKLIGHT_MODE_AUTO:
			displayLightTrigger();
			break;
	}
}

static void setDisplayInvert(bool invert)
{
	if (invert == nonVolatileSettings.displayInverseVideo)
	{
		return;// Don't update unless the setting is actually changing
	}

	bool isLit = displayIsBacklightLit();

	settingsSet(nonVolatileSettings.displayInverseVideo, invert);//!nonVolatileSettings.displayInverseVideo;
	displayInit(nonVolatileSettings.displayInverseVideo);// Need to perform a full reset on the display to change back to non-inverted
	// Need to cycle the backlight
	if (nonVolatileSettings.backlightMode != BACKLIGHT_MODE_NONE)
	{
		displayEnableBacklight(!isLit);
		displayEnableBacklight(isLit);
	}
}

static void checkMinBacklightValue(void)
{
	if (nonVolatileSettings.displayBacklightPercentageOff >= nonVolatileSettings.displayBacklightPercentage)
	{
		settingsSet(nonVolatileSettings.displayBacklightPercentageOff,
				nonVolatileSettings.displayBacklightPercentage ?
						(nonVolatileSettings.displayBacklightPercentage - ((nonVolatileSettings.displayBacklightPercentageOff <= BACKLIGHT_PERCENTAGE_STEP) ? BACKLIGHT_PERCENTAGE_STEP_SMALL : BACKLIGHT_PERCENTAGE_STEP)) : 0);
	}
}
