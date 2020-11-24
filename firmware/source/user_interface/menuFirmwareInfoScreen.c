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
#include <user_interface/menuSystem.h>
#include <user_interface/uiLocalisation.h>
#include <user_interface/uiUtilities.h>

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);

menuStatus_t menuFirmwareInfoScreen(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		updateScreen();
	}
	else
	{
		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return MENU_STATUS_SUCCESS;
}

static void updateScreen(void)
{
#if !defined(PLATFORM_GD77S)
	char buf[17];
	char * const *radioModel;

	snprintf(buf, 16, "v1.78.20");
	buf[9] = 0; // git hash id 7 char long;
//	strcat(buf, "]");

	ucClearBuf();

#if defined(PLATFORM_GD77)
	radioModel = (char * const *)&currentLanguage->openGD77;
#elif defined(PLATFORM_DM1801)
	radioModel = (char * const *)&currentLanguage->openDM1801;
#elif defined(PLATFORM_RD5R)
	radioModel = (char * const *)&currentLanguage->openRD5R;
#endif

#if defined(PLATFORM_RD5R)
	ucPrintCentered(2, *radioModel, FONT_SIZE_3);
#else
	ucPrintCentered(5, *radioModel, FONT_SIZE_3);
#endif



#if defined(PLATFORM_RD5R)
	ucPrintCentered(14, "Built by NA7Q", FONT_SIZE_2);
	ucPrintCentered(24,__TIME__, FONT_SIZE_2);
	ucPrintCentered(32,__DATE__, FONT_SIZE_2);
	ucPrintCentered(40, buf, FONT_SIZE_2);
#else
	ucPrintCentered(24, "Built by NA7Q", FONT_SIZE_2);
	ucPrintCentered(34,__TIME__, FONT_SIZE_2);
	ucPrintCentered(44,__DATE__, FONT_SIZE_2);
	ucPrintCentered(54, buf, FONT_SIZE_2);

#endif

	voicePromptsInit();
	voicePromptsAppendLanguageString((const char * const *)radioModel);
	voicePromptsAppendLanguageString(&currentLanguage->built);
	voicePromptsAppendString(__TIME__);
	voicePromptsAppendString(__DATE__);
	voicePromptsAppendLanguageString(&currentLanguage->gitCommit);
	voicePromptsAppendString(buf);
	voicePromptsPlay();

	ucRender();
	displayLightTrigger();
#endif
}


static void handleEvent(uiEvent_t *ev)
{
	displayLightTrigger();

	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		menuSystemPopAllAndDisplayRootMenu();
		return;
	}
}
