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
#include <settings.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiLocalisation.h>
#include <user_interface/uiUtilities.h>
#include <wdog.h>

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);

static menuStatus_t menuOptionsExitCode = MENU_STATUS_SUCCESS;
static bool	doFactoryReset;
enum OPTIONS_MENU_LIST { OPTIONS_MENU_HOTSPOT_TYPE = 0, OPTIONS_MENU_FACTORY_RESET, OPTIONS_MENU_USE_CALIBRATION,
							OPTIONS_MENU_TX_FREQ_LIMITS,
							OPTIONS_MENU_KEYPAD_TIMER_LONG, OPTIONS_MENU_KEYPAD_TIMER_REPEAT, OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT,
							OPTIONS_MENU_SCAN_DELAY, OPTIONS_MENU_SCAN_MODE,
							OPTIONS_MENU_SQUELCH_DEFAULT_VHF, OPTIONS_MENU_SQUELCH_DEFAULT_220MHz, OPTIONS_MENU_SQUELCH_DEFAULT_UHF,
							OPTIONS_MENU_PTT_TOGGLE, OPTIONS_MENU_TALKER_ALIAS_TX,
							OPTIONS_MENU_PRIVATE_CALLS,
							NUM_OPTIONS_MENU_ITEMS};

menuStatus_t menuOptions(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		doFactoryReset = false;
		// Store original settings, used on cancel event.
		memcpy(&originalNonVolatileSettings, &nonVolatileSettings, sizeof(settingsStruct_t));

		if (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1)
		{
			voicePromptsInit();
			voicePromptsAppendPrompt(PROMPT_SILENCE);
			voicePromptsAppendPrompt(PROMPT_SILENCE);
			voicePromptsAppendLanguageString(&currentLanguage->options);
			voicePromptsAppendLanguageString(&currentLanguage->menu);
			voicePromptsAppendPrompt(PROMPT_SILENCE);
			voicePromptsAppendPrompt(PROMPT_SILENCE);
		}

		updateScreen(true);
		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuOptionsExitCode = MENU_STATUS_SUCCESS;

		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return menuOptionsExitCode;
}

static void updateScreen(bool isFirstRun)
{
	int mNum = 0;
	static const int bufferLen = 17;
	char buf[bufferLen];
	char * const *leftSide = NULL;// initialise to please the compiler
	char * const *rightSideConst = NULL;// initialise to please the compiler
	char rightSideVar[bufferLen];

	ucClearBuf();
	menuDisplayTitle(currentLanguage->options);

	// Can only display 3 of the options at a time menu at -1, 0 and +1
	for(int i = -1; i <= 1; i++)
	{
		mNum = menuGetMenuOffset(NUM_OPTIONS_MENU_ITEMS, i);
		buf[0] = 0;
		rightSideVar[0] = 0;

		switch(mNum)
		{
			case OPTIONS_MENU_HOTSPOT_TYPE:
				leftSide = (char * const *)&currentLanguage->hotspot_mode;
#if defined(PLATFORM_RD5R)
				rightSideConst = (char * const *)&currentLanguage->n_a;
#else
				{
					const char *hsTypes[] = {"MMDVM", "BlueDV" };
					if (nonVolatileSettings.hotspotType == 0)
					{
						rightSideConst = (char * const *)&currentLanguage->off;
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "%s", hsTypes[nonVolatileSettings.hotspotType - 1]);
					}
				}
#endif
				break;
			case OPTIONS_MENU_FACTORY_RESET:
				leftSide = (char * const *)&currentLanguage->factory_reset;
				rightSideConst = (char * const *)(doFactoryReset ? &currentLanguage->yes : &currentLanguage->no);
				break;
			case OPTIONS_MENU_USE_CALIBRATION:
				leftSide = (char * const *)&currentLanguage->calibration;
				rightSideConst = (char * const *)(nonVolatileSettings.useCalibration ? &currentLanguage->on : &currentLanguage->off);
				break;
			case OPTIONS_MENU_TX_FREQ_LIMITS:// Tx Freq limits
				leftSide = (char * const *)&currentLanguage->band_limits;
				rightSideConst = (char * const *)(nonVolatileSettings.txFreqLimited ? &currentLanguage->on : &currentLanguage->off);
				break;
			case OPTIONS_MENU_KEYPAD_TIMER_LONG:// Timer longpress
				leftSide = (char * const *)&currentLanguage->key_long;
				snprintf(rightSideVar, bufferLen, "%1d.%1ds", nonVolatileSettings.keypadTimerLong / 10, nonVolatileSettings.keypadTimerLong % 10);
				break;
			case OPTIONS_MENU_KEYPAD_TIMER_REPEAT:// Timer repeat
				leftSide = (char * const *)&currentLanguage->key_repeat;
				snprintf(rightSideVar, bufferLen, "%1d.%1ds", nonVolatileSettings.keypadTimerRepeat/10, nonVolatileSettings.keypadTimerRepeat % 10);
				break;
			case OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT:// DMR filtr timeout repeat
				leftSide = (char * const *)&currentLanguage->dmr_filter_timeout;
				snprintf(rightSideVar, bufferLen, "%ds", nonVolatileSettings.dmrCaptureTimeout);
				break;
			case OPTIONS_MENU_SCAN_DELAY:// Scan hold and pause time
				leftSide = (char * const *)&currentLanguage->scan_delay;
				snprintf(rightSideVar, bufferLen, "%ds", nonVolatileSettings.scanDelay);
				break;
			case OPTIONS_MENU_SCAN_MODE:// scanning mode
				leftSide = (char * const *)&currentLanguage->scan_mode;
				{
					const char * const *scanModes[] = { &currentLanguage->hold, &currentLanguage->pause, &currentLanguage->stop };
					rightSideConst = (char * const *)scanModes[nonVolatileSettings.scanModePause];
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_VHF:
				leftSide = (char * const *)&currentLanguage->squelch_VHF;
				snprintf(rightSideVar, bufferLen, "%d%%", (nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF] - 1) * 5);// 5% steps
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_220MHz:
				leftSide = (char * const *)&currentLanguage->squelch_220;
				snprintf(rightSideVar, bufferLen, "%d%%", (nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz] - 1) * 5);// 5% steps
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_UHF:
				leftSide = (char * const *)&currentLanguage->squelch_UHF;
				snprintf(rightSideVar, bufferLen, "%d%%", (nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF] - 1) * 5);// 5% steps
				break;
			case OPTIONS_MENU_PTT_TOGGLE:
				leftSide = (char * const *)&currentLanguage->ptt_toggle;
				rightSideConst = (char * const *)(nonVolatileSettings.pttToggle ? &currentLanguage->on : &currentLanguage->off);
				break;

			case OPTIONS_MENU_TALKER_ALIAS_TX:
				leftSide = (char * const *)&currentLanguage->transmitTalkerAlias;
				rightSideConst = (char * const *)(nonVolatileSettings.transmitTalkerAlias ? &currentLanguage->on : &currentLanguage->off);
				break;
			case OPTIONS_MENU_PRIVATE_CALLS:
				leftSide = (char * const *)&currentLanguage->private_call_handling;
				const char * const *allowPCOptions[] = { &currentLanguage->off, &currentLanguage->on, &currentLanguage->ptt, &currentLanguage->Auto};
				rightSideConst = (char * const *)allowPCOptions[nonVolatileSettings.privateCalls];
				break;
		}

		snprintf(buf, bufferLen, "%s:%s", *leftSide, (rightSideVar[0] ? rightSideVar : *rightSideConst));

		if ((i == 0) && (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1))
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

	if (KEYCHECK_PRESS(ev->keys,KEY_DOWN))
	{
		isDirty = true;
		menuSystemMenuIncrement(&gMenusCurrentItemIndex, NUM_OPTIONS_MENU_ITEMS);
		menuOptionsExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		isDirty = true;
		menuSystemMenuDecrement(&gMenusCurrentItemIndex, NUM_OPTIONS_MENU_ITEMS);
		menuOptionsExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_RIGHT))
	{
		isDirty = true;
		switch(gMenusCurrentItemIndex)
		{
			case OPTIONS_MENU_FACTORY_RESET:
				doFactoryReset = true;
				break;
			case OPTIONS_MENU_USE_CALIBRATION:
				settingsSet(nonVolatileSettings.useCalibration, true);
				break;
			case OPTIONS_MENU_TX_FREQ_LIMITS:
				settingsSet(nonVolatileSettings.txFreqLimited, true);
				break;
			case OPTIONS_MENU_KEYPAD_TIMER_LONG:
				if (nonVolatileSettings.keypadTimerLong < 90)
				{
					settingsIncrement(nonVolatileSettings.keypadTimerLong, 1);
				}
				break;
			case OPTIONS_MENU_KEYPAD_TIMER_REPEAT:
				if (nonVolatileSettings.keypadTimerRepeat < 90)
				{
					settingsIncrement(nonVolatileSettings.keypadTimerRepeat, 1);
				}
				break;
			case OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT:
				if (nonVolatileSettings.dmrCaptureTimeout < 90)
				{
					settingsIncrement(nonVolatileSettings.dmrCaptureTimeout, 1);
				}
				break;
			case OPTIONS_MENU_SCAN_DELAY:
				if (nonVolatileSettings.scanDelay < 30)
				{
					settingsIncrement(nonVolatileSettings.scanDelay, 1);
				}
				break;
			case OPTIONS_MENU_SCAN_MODE:
				if (nonVolatileSettings.scanModePause < SCAN_MODE_STOP)
				{
					settingsIncrement(nonVolatileSettings.scanModePause, 1);
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_VHF:
				if (nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF] < CODEPLUG_MAX_VARIABLE_SQUELCH)
				{
					settingsIncrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF], 1);
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_220MHz:
				if (nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz] < CODEPLUG_MAX_VARIABLE_SQUELCH)
				{
					settingsIncrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz], 1);
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_UHF:
				if (nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF] < CODEPLUG_MAX_VARIABLE_SQUELCH)
				{
					settingsIncrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF], 1);
				}
				break;
			case OPTIONS_MENU_PTT_TOGGLE:
				settingsSet(nonVolatileSettings.pttToggle, true);
				break;
			case OPTIONS_MENU_HOTSPOT_TYPE:
#if !defined(PLATFORM_RD5R)
				if (nonVolatileSettings.hotspotType < HOTSPOT_TYPE_BLUEDV)
				{
					settingsIncrement(nonVolatileSettings.hotspotType, 1);
				}
#endif
				break;
			case OPTIONS_MENU_TALKER_ALIAS_TX:
				settingsSet(nonVolatileSettings.transmitTalkerAlias, true);
				break;
			case OPTIONS_MENU_PRIVATE_CALLS:
				// Note. Currently the "AUTO" option is not available
				if (nonVolatileSettings.privateCalls < ALLOW_PRIVATE_CALLS_PTT)
				{
					settingsIncrement(nonVolatileSettings.privateCalls, 1);
				}
				break;
		}
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_LEFT))
	{
		isDirty = true;
		switch(gMenusCurrentItemIndex)
		{
			case OPTIONS_MENU_HOTSPOT_TYPE:
#if !defined(PLATFORM_RD5R)
				if (nonVolatileSettings.hotspotType > HOTSPOT_TYPE_OFF)
				{
					settingsDecrement(nonVolatileSettings.hotspotType, 1);
				}
#endif
				break;
			case OPTIONS_MENU_FACTORY_RESET:
				doFactoryReset = false;
				break;
			case OPTIONS_MENU_USE_CALIBRATION:
				settingsSet(nonVolatileSettings.useCalibration, false);
				break;
			case OPTIONS_MENU_TX_FREQ_LIMITS:
				settingsSet(nonVolatileSettings.txFreqLimited, false);
				break;
			case OPTIONS_MENU_KEYPAD_TIMER_LONG:
				if (nonVolatileSettings.keypadTimerLong > 1)
				{
					settingsDecrement(nonVolatileSettings.keypadTimerLong, 1);
				}
				break;
			case OPTIONS_MENU_KEYPAD_TIMER_REPEAT:
				if (nonVolatileSettings.keypadTimerRepeat > 1) // Don't set it to zero, otherwise watchdog may kicks in.
				{
					settingsDecrement(nonVolatileSettings.keypadTimerRepeat, 1);
				}
				break;
			case OPTIONS_MENU_DMR_MONITOR_CAPTURE_TIMEOUT:
				if (nonVolatileSettings.dmrCaptureTimeout > 1)
				{
					settingsDecrement(nonVolatileSettings.dmrCaptureTimeout, 1);
				}
				break;
			case OPTIONS_MENU_SCAN_DELAY:
				if (nonVolatileSettings.scanDelay > 1)
				{
					settingsDecrement(nonVolatileSettings.scanDelay, 1);
				}
				break;
			case OPTIONS_MENU_SCAN_MODE:
				if (nonVolatileSettings.scanModePause > SCAN_MODE_HOLD)
				{
					settingsDecrement(nonVolatileSettings.scanModePause, 1);
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_VHF:
				if (nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF] > 1)
				{
					settingsDecrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF], 1);
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_220MHz:
				if (nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz] > 1)
				{
					settingsDecrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz], 1);
				}
				break;
			case OPTIONS_MENU_SQUELCH_DEFAULT_UHF:
				if (nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF] > 1)
				{
					settingsDecrement(nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF], 1);
				}
				break;
			case OPTIONS_MENU_PTT_TOGGLE:
				settingsSet(nonVolatileSettings.pttToggle, false);
				break;
			case OPTIONS_MENU_TALKER_ALIAS_TX:
				settingsSet(nonVolatileSettings.transmitTalkerAlias, false);
				break;
			case OPTIONS_MENU_PRIVATE_CALLS:
				if (nonVolatileSettings.privateCalls > 0)
				{
					settingsDecrement(nonVolatileSettings.privateCalls, 1);
				}
				break;
		}
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		if (doFactoryReset == true)
		{
			settingsRestoreDefaultSettings();
			watchdogReboot();
		}

		settingsSaveIfNeeded(true);
		menuSystemPopAllAndDisplayRootMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		// Restore original settings.
		memcpy(&nonVolatileSettings, &originalNonVolatileSettings, sizeof(settingsStruct_t));
		settingsSaveIfNeeded(true);
		menuSystemPopPreviousMenu();
		return;
	}

	if (isDirty)
	{
		updateScreen(false);
	}
}
