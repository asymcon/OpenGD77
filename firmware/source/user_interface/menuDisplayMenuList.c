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
#include <main.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiLocalisation.h>

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);

static menuStatus_t menuDisplayListExitCode = MENU_STATUS_SUCCESS;

menuStatus_t menuDisplayMenuList(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		int currentMenuNumber = menuSystemGetCurrentMenuNumber();
		gMenuCurrentMenuList = (menuItemNewData_t *)menusData[currentMenuNumber]->items;
		gMenusEndIndex = menusData[currentMenuNumber]->numItems;
		if (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1)
		{
			voicePromptsInit();
			voicePromptsAppendLanguageString(&currentLanguage->menu);
			voicePromptsAppendPrompt(PROMPT_SILENCE);
		}
		updateScreen(true);

		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuDisplayListExitCode = MENU_STATUS_SUCCESS;

		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return menuDisplayListExitCode;
}

static void updateScreen(bool isFirstRun)
{
	int mNum;

	ucClearBuf();
	menuDisplayTitle(currentLanguage->menu);

	for(int i = -1; i <= 1 ; i++)
	{
		mNum = menuGetMenuOffset(gMenusEndIndex, i);

		if (mNum < gMenusEndIndex)
		{
			if (gMenuCurrentMenuList[mNum].stringOffset >= 0)
			{
				char **menuName = (char **)((int)&currentLanguage->LANGUAGE_NAME + (gMenuCurrentMenuList[mNum].stringOffset * sizeof(char *)));
				menuDisplayEntry(i, mNum, (const char *)*menuName);
				if ((nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1) && (i==0))
				{
					if (!isFirstRun)
					{
						voicePromptsInit();
					}
					voicePromptsAppendLanguageString((const char * const *)menuName);
					voicePromptsPlay();
				}
			}
		}
	}

	ucRender();
	displayLightTrigger();
}

static void handleEvent(uiEvent_t *ev)
{
	displayLightTrigger();

	if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		menuSystemMenuIncrement(&gMenusCurrentItemIndex, gMenusEndIndex);
		updateScreen(false);
		menuDisplayListExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		menuSystemMenuDecrement(&gMenusCurrentItemIndex, gMenusEndIndex);
		updateScreen(false);
		menuDisplayListExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		if (gMenuCurrentMenuList[gMenusCurrentItemIndex].menuNum!=-1)
		{
			menuSystemPushNewMenu(gMenuCurrentMenuList[gMenusCurrentItemIndex].menuNum);
		}
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_STAR) && (menuSystemGetCurrentMenuNumber() == MENU_MAIN_MENU))
	{
		keypadLocked = true;
		menuSystemPopAllAndDisplayRootMenu();
		menuSystemPushNewMenu(UI_LOCK_SCREEN);
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_HASH) && (menuSystemGetCurrentMenuNumber() == MENU_MAIN_MENU))
	{
		PTTLocked = !PTTLocked;
		menuSystemPopAllAndDisplayRootMenu();
		menuSystemPushNewMenu(UI_LOCK_SCREEN);
		return;
	}
}
