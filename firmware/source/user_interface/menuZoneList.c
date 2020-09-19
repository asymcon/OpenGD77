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
#include <codeplug.h>
#include <main.h>
#include <settings.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiLocalisation.h>

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);

static menuStatus_t menuZoneExitCode = MENU_STATUS_SUCCESS;

menuStatus_t menuZoneList(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		gMenusEndIndex = codeplugZonesGetCount();
		gMenusCurrentItemIndex = nonVolatileSettings.currentZone;
		if (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1)
		{
			voicePromptsInit();
			voicePromptsAppendLanguageString(&currentLanguage->zone);
			voicePromptsAppendPrompt(PROMPT_SILENCE);
		}
		updateScreen(true);
		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuZoneExitCode = MENU_STATUS_SUCCESS;

		if (ev->hasEvent)
			handleEvent(ev);
	}
	return menuZoneExitCode;
}

static void updateScreen(bool isFirstRun)
{
	char nameBuf[17];
	int mNum;
	struct_codeplugZone_t zoneBuf;

	ucClearBuf();
	menuDisplayTitle(currentLanguage->zones);

	for(int i = -1; i <= 1; i++)
	{
		if (gMenusEndIndex <= (i + 1))
		{
			break;
		}

		mNum = menuGetMenuOffset(gMenusEndIndex, i);

		codeplugZoneGetDataForNumber(mNum, &zoneBuf);
		codeplugUtilConvertBufToString(zoneBuf.name, nameBuf, 16);// need to convert to zero terminated string

		menuDisplayEntry(i, mNum, (char* )nameBuf);

		if ((nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1) && (i==0))
		{
			if (!isFirstRun)
			{
				voicePromptsInit();
			}

			if (strcmp(nameBuf,currentLanguage->all_channels)==0)
			{
				voicePromptsAppendLanguageString(&currentLanguage->all_channels);
			}
			else
			{
				voicePromptsAppendString(nameBuf);
			}

			voicePromptsPlay();
		}
	}

	ucRender();
	displayLightTrigger();
}

static void handleEvent(uiEvent_t *ev)
{
	displayLightTrigger();

	if (BUTTONCHECK_SHORTUP(ev, BUTTON_SK1))
	{
		voicePromptsPlay();
	}

	if (KEYCHECK_PRESS(ev->keys,KEY_DOWN))
	{
		menuSystemMenuIncrement(&gMenusCurrentItemIndex, gMenusEndIndex);
		updateScreen(false);
		menuZoneExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_PRESS(ev->keys,KEY_UP))
	{
		menuSystemMenuDecrement(&gMenusCurrentItemIndex, gMenusEndIndex);
		updateScreen(false);
		menuZoneExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_GREEN))
	{
		nonVolatileSettings.overrideTG = 0; // remove any TG override
		nonVolatileSettings.currentZone = gMenusCurrentItemIndex;
		nonVolatileSettings.currentChannelIndexInZone = 0;// Since we are switching zones the channel index should be reset
		nonVolatileSettings.currentIndexInTRxGroupList[SETTINGS_CHANNEL_MODE]=0;// Since we are switching zones the TRx Group index should be reset
		channelScreenChannelData.rxFreq=0x00; // Flag to the Channel screen that the channel data is now invalid and needs to be reloaded
		menuSystemPopAllAndDisplaySpecificRootMenu(UI_CHANNEL_MODE, true);
		SETTINGS_PLATFORM_SPECIFIC_SAVE_SETTINGS(false);// For Baofeng RD-5R

		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
}
