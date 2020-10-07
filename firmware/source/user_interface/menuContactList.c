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
#include <user_interface/uiUtilities.h>
#include <user_interface/uiLocalisation.h>

static void updateScreen(bool isFirstRun);
static void handleEvent(uiEvent_t *ev);
static void dtmfPrepareSequence(uint8_t *seq);
static void dtmfProcess(void);
static struct_codeplugContact_t     contact;
static struct_codeplugDTMFContact_t dtmfContact;
static int listType;
static int contactCallType;
static int menuContactListDisplayState;
static int menuContactListTimeout;
static int menuContactListOverrideState = 0;
static menuStatus_t menuContactListExitCode = MENU_STATUS_SUCCESS;

static bool dtmfKeying = false;
static uint8_t dtmfBuffer[17U]; // 16 tones + final time-length
static uint8_t dtmfpoLen = 0U;
static uint8_t dtmfpoPtr = 0U;
static const uint32_t dtmfDuration = (10U * 75U); // 75ms per tone
static const uint32_t dtmfPauseDuration = (10U * 50U); // 50ms per pause
static const uint32_t pretimeDuration = (10U * 200U); // 200ms pretime
static uint32_t dtmfNextPeriod = 0;
static uint32_t dtmfPausePeriod = 0;
static uint32_t pretimePeriod;
static bool dtmfPaused = false;
static bool pretimeOver = false;

enum MENU_CONTACT_LIST_STATE
{
	MENU_CONTACT_LIST_DISPLAY = 0,
	MENU_CONTACT_LIST_CONFIRM,
	MENU_CONTACT_LIST_DELETED,
	MENU_CONTACT_LIST_TG_IN_RXGROUP
};

enum MENU_CONTACT_LIST_CONTACT_TYPE
{
	MENU_CONTACT_LIST_CONTACT_DIGITAL = 0,
	MENU_CONTACT_LIST_CONTACT_DTMF
};

static void reloadContactList(int type)
{
	gMenusEndIndex = (type == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? codeplugContactsGetCount(contactCallType) : codeplugDTMFContactsGetCount();

	if (gMenusEndIndex > 0)
	{
		if (gMenusCurrentItemIndex >= gMenusEndIndex)
		{
			gMenusCurrentItemIndex = 0;
		}
		contactListContactIndex = (type == MENU_CONTACT_LIST_CONTACT_DIGITAL)
				? codeplugContactGetDataForNumber(gMenusCurrentItemIndex + 1, contactCallType, &contactListContactData)
				: codeplugDTMFContactGetDataForNumber(gMenusCurrentItemIndex + 1, &contactListDTMFContactData);
	}
	else
	{
		contactListContactIndex = 0;
	}
}

menuStatus_t menuContactList(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		if (menuContactListOverrideState == 0)
		{
			if (contactListContactIndex == 0)
			{
				int currentMenu = menuSystemGetCurrentMenuNumber();

				// Shows digital contact list if called from "contact list" menu entry, or from <SK2>+# in digital.
				// Otherwise displays DTMF contact list
				listType = ((currentMenu == MENU_CONTACT_LIST) || ((currentMenu == MENU_CONTACT_QUICKLIST) && (trxGetMode() != RADIO_MODE_ANALOG))) ? MENU_CONTACT_LIST_CONTACT_DIGITAL : MENU_CONTACT_LIST_CONTACT_DTMF;
				contactCallType = CONTACT_CALLTYPE_TG;
				dtmfKeying = false;
				dtmfpoLen = 0U;
			}
			else
			{
				if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
				{
					codeplugContactGetDataForIndex(contactListContactIndex, &contactListContactData);
					contactCallType = contactListContactData.callType;
				}
				else
				{
					codeplugDTMFContactGetDataForIndex(contactListContactIndex, &contactListDTMFContactData);
				}
			}

			reloadContactList(listType);
			menuContactListDisplayState = MENU_CONTACT_LIST_DISPLAY;

			voicePromptsInit();
			voicePromptsAppendPrompt(PROMPT_SILENCE);
			voicePromptsAppendPrompt(PROMPT_SILENCE);
			if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
			{
				voicePromptsAppendLanguageString(&currentLanguage->contacts);
				voicePromptsAppendLanguageString(&currentLanguage->menu);
				voicePromptsAppendPrompt(PROMPT_SILENCE);
				voicePromptsAppendPrompt(PROMPT_SILENCE);
			}
			else
			{
				if (menuSystemGetCurrentMenuNumber() == MENU_CONTACT_QUICKLIST)
				{
					voicePromptsAppendLanguageString(&currentLanguage->dtmf_contact_list);
					voicePromptsAppendPrompt(PROMPT_SILENCE);
					voicePromptsAppendPrompt(PROMPT_SILENCE);
				}
			}
		}
		else
		{
			if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
			{
				codeplugContactGetDataForIndex(contactListContactIndex, &contactListContactData);
			}
			else
			{
				codeplugDTMFContactGetDataForIndex(contactListContactIndex, &contactListDTMFContactData);
			}
			menuContactListDisplayState = menuContactListOverrideState;
			menuContactListOverrideState = 0;
		}

		updateScreen(true);
		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuContactListExitCode = MENU_STATUS_SUCCESS;

		if (ev->hasEvent || (menuContactListTimeout > 0))
		{
			handleEvent(ev);
		}

	}
	return menuContactListExitCode;
}

static void updateScreen(bool isFirstRun)
{
	char nameBuf[33];
	int mNum;
	int idx;
	const char *calltypeName[] = { currentLanguage->group_call, currentLanguage->private_call, currentLanguage->all_call, "DTMF" };

	ucClearBuf();

	switch (menuContactListDisplayState)
	{
	case MENU_CONTACT_LIST_DISPLAY:
		menuDisplayTitle((char *) calltypeName[((listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? contactCallType : 3)]);

		if (!isFirstRun)
		{
			voicePromptsInit();
		}

		if (gMenusEndIndex == 0)
		{
			ucPrintCentered((DISPLAY_SIZE_Y / 2), currentLanguage->empty_list, FONT_SIZE_3);

			voicePromptsAppendLanguageString(&currentLanguage->empty_list);
			voicePromptsPlay();
		}
		else
		{
			for (int i = -1; i <= 1; i++)
			{
				mNum = menuGetMenuOffset(gMenusEndIndex, i);
				idx = (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
						? codeplugContactGetDataForNumber(mNum + 1, contactCallType, &contact)
						: codeplugDTMFContactGetDataForNumber(mNum + 1, &dtmfContact);

				if (idx > 0)
				{
					codeplugUtilConvertBufToString(((listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? contact.name : dtmfContact.name), nameBuf, 16); // need to convert to zero terminated string
					menuDisplayEntry(i, mNum, (char*) nameBuf);
				}

				if (i == 0)
				{
					voicePromptsAppendString(nameBuf);
					voicePromptsPlay();
				}
			}
		}
		break;
	case MENU_CONTACT_LIST_CONFIRM:
		codeplugUtilConvertBufToString(contactListContactData.name, nameBuf, 16);
		menuDisplayTitle(nameBuf);
		ucPrintCentered(16, currentLanguage->delete_contact_qm, FONT_SIZE_3);
		ucDrawChoice(CHOICE_YESNO, false);
		break;
	case MENU_CONTACT_LIST_DELETED:
		codeplugUtilConvertBufToString(contactListContactData.name, nameBuf, 16);
		ucPrintCentered(16, currentLanguage->contact_deleted, FONT_SIZE_3);
		ucDrawChoice(CHOICE_DISMISS, false);
		break;
	case MENU_CONTACT_LIST_TG_IN_RXGROUP:
		codeplugUtilConvertBufToString(contactListContactData.name, nameBuf, 16);
		menuDisplayTitle(nameBuf);
		ucPrintCentered(16, currentLanguage->contact_used, FONT_SIZE_3);
		ucPrintCentered((DISPLAY_SIZE_Y/2), currentLanguage->in_rx_group, FONT_SIZE_3);
		ucDrawChoice(CHOICE_DISMISS, false);
		break;
	}

	ucRender();
	displayLightTrigger();
}

static void handleEvent(uiEvent_t *ev)
{
	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	switch (menuContactListDisplayState)
	{
		case MENU_CONTACT_LIST_DISPLAY:
			if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
			{
				menuSystemMenuIncrement(&gMenusCurrentItemIndex, gMenusEndIndex);
				contactListContactIndex = (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
						? codeplugContactGetDataForNumber(gMenusCurrentItemIndex + 1, contactCallType, &contactListContactData)
						: codeplugDTMFContactGetDataForNumber(gMenusCurrentItemIndex + 1, &contactListDTMFContactData);
				updateScreen(false);
				menuContactListExitCode |= MENU_STATUS_LIST_TYPE;
			}
			else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
			{
				menuSystemMenuDecrement(&gMenusCurrentItemIndex, gMenusEndIndex);
				contactListContactIndex = (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
						? codeplugContactGetDataForNumber(gMenusCurrentItemIndex + 1, contactCallType, &contactListContactData)
						: codeplugDTMFContactGetDataForNumber(gMenusCurrentItemIndex + 1, &contactListDTMFContactData);
				updateScreen(false);
				menuContactListExitCode |= MENU_STATUS_LIST_TYPE;
			}
			else if (KEYCHECK_SHORTUP(ev->keys, KEY_HASH))
			{
				if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
				{
					if (contactCallType == CONTACT_CALLTYPE_TG)
					{
						contactCallType = CONTACT_CALLTYPE_PC;
					}
					else
					{
						contactCallType = CONTACT_CALLTYPE_TG;
					}
					reloadContactList(listType);
					updateScreen(false);
				}
			}
			else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
			{
				if (menuSystemGetCurrentMenuNumber() == MENU_CONTACT_QUICKLIST)
				{
					if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
					{
						if (gMenusEndIndex > 0)
						{
							setOverrideTGorPC(contactListContactData.tgNumber, contactListContactData.callType == CONTACT_CALLTYPE_PC);
							tsSetContactOverride(((menuSystemGetRootMenuNumber() == UI_CHANNEL_MODE) ? CHANNEL_CHANNEL : (CHANNEL_VFO_A + nonVolatileSettings.currentVFONumber)), &contactListContactData);
							contactListContactIndex = 0;
							announceItem(PROMPT_SEQUENCE_CONTACT_TG_OR_PC, PROMPT_THRESHOLD_3);
							menuSystemPopAllAndDisplayRootMenu();
						}
						return;
					}
				}

				// Display submenu for DTMF contact list
				if (gMenusEndIndex > 0) // display action list only if contact list is non empty
				{
					menuSystemPushNewMenu(MENU_CONTACT_LIST_SUBMENU);
				}
				return;
			}
			else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
			{
				contactListContactIndex = 0;
				menuSystemPopPreviousMenu();
				return;
			}
			break;

		case MENU_CONTACT_LIST_CONFIRM:
			if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
			{
				memset(contact.name, 0xff, 16);
				contact.tgNumber = 0;
				contact.callType = 0xFF;
				codeplugContactSaveDataForIndex(contactListContactIndex, &contact);
				contactListContactIndex = 0;
				menuContactListTimeout = 2000;
				menuContactListDisplayState = MENU_CONTACT_LIST_DELETED;
				reloadContactList(listType);
				updateScreen(false);
				voicePromptsAppendLanguageString(&currentLanguage->contact_deleted);
				voicePromptsPlay();
			}
			else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
			{
				menuContactListDisplayState = MENU_CONTACT_LIST_DISPLAY;
				reloadContactList(listType);
				updateScreen(false);
			}
			break;

		case MENU_CONTACT_LIST_DELETED:
		case MENU_CONTACT_LIST_TG_IN_RXGROUP:
			menuContactListTimeout--;
			if ((menuContactListTimeout == 0) || KEYCHECK_SHORTUP(ev->keys, KEY_GREEN) || KEYCHECK_SHORTUP(ev->keys, KEY_RED))
			{
				menuContactListDisplayState = MENU_CONTACT_LIST_DISPLAY;
				reloadContactList(listType);
			}
			updateScreen(false);
			break;
	}
}

enum CONTACT_LIST_QUICK_MENU_ITEMS
{
	CONTACT_LIST_QUICK_MENU_SELECT = 0,
	CONTACT_LIST_QUICK_MENU_EDIT,
	CONTACT_LIST_QUICK_MENU_DELETE,
	NUM_CONTACT_LIST_QUICK_MENU_ITEMS    // The last item in the list is used so that we automatically get a total number of items in the list
};

static void updateSubMenuScreen(void)
{
	int mNum = 0;
	static const int bufferLen = 17;
	char buf[bufferLen];
	char * const *langTextConst = NULL;// initialise to please the compiler

	voicePromptsInit();

	ucClearBuf();

	codeplugUtilConvertBufToString((listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? contactListContactData.name : contactListDTMFContactData.name, buf, 16);
	menuDisplayTitle(buf);

	for(int i = -1; i <= 1; i++)
	{
		mNum = menuGetMenuOffset(((listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? NUM_CONTACT_LIST_QUICK_MENU_ITEMS : 1), i);

		switch(mNum)
		{
			case CONTACT_LIST_QUICK_MENU_SELECT:
				langTextConst = (char * const *)&currentLanguage->select_tx;
				break;
			case CONTACT_LIST_QUICK_MENU_EDIT:
				langTextConst = (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? (char * const *)&currentLanguage->edit_contact : NULL;
				break;
			case CONTACT_LIST_QUICK_MENU_DELETE:
				langTextConst = (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? (char * const *)&currentLanguage->delete_contact : NULL;
				break;
		}

		if (langTextConst != NULL)
		{
			strncpy(buf, *langTextConst, 17);
		}
		else
		{
			strncpy(buf, " ", 17);
		}

		if ((i == 0) && (langTextConst != NULL))
		{
			voicePromptsAppendLanguageString((const char * const *)langTextConst);
			voicePromptsPlay();
		}

		buf[bufferLen - 1] = 0;
		menuDisplayEntry(i, mNum, buf);
	}

	ucRender();
	displayLightTrigger();
}

static void handleSubMenuEvent(uiEvent_t *ev)
{
	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	// DTMF sequence is playing, stop it.
	if (dtmfKeying && ((ev->keys.key != 0) || BUTTONCHECK_DOWN(ev, BUTTON_PTT)
#if ! defined(PLATFORM_RD5R)
			                               || BUTTONCHECK_DOWN(ev, BUTTON_ORANGE)
#endif
	))
	{
		dtmfpoLen = 0U;
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		menuContactListOverrideState = 0;
		switch (gMenusCurrentItemIndex)
		{
			case CONTACT_LIST_QUICK_MENU_SELECT:
				if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
				{
					setOverrideTGorPC(contactListContactData.tgNumber, (contactListContactData.callType == CONTACT_CALLTYPE_PC));
					tsSetContactOverride(((menuSystemGetRootMenuNumber() == UI_CHANNEL_MODE) ? CHANNEL_CHANNEL : (CHANNEL_VFO_A + nonVolatileSettings.currentVFONumber)), &contactListContactData);
					contactListContactIndex = 0;
					announceItem(PROMPT_SEQUENCE_CONTACT_TG_OR_PC, PROMPT_THRESHOLD_3);
					inhibitInitialVoicePrompt = true;
					menuSystemPopAllAndDisplayRootMenu();
				}
				else
				{
					dtmfPrepareSequence(contactListDTMFContactData.code);
				}
				break;
			case CONTACT_LIST_QUICK_MENU_EDIT:
				if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
				{
					menuSystemSetCurrentMenu(MENU_CONTACT_DETAILS);
				}
				break;
			case CONTACT_LIST_QUICK_MENU_DELETE:
				if (contactListContactIndex > 0)
				{
					if (listType == MENU_CONTACT_LIST_CONTACT_DIGITAL)
					{
						if ((contactListContactData.callType == CONTACT_CALLTYPE_TG) &&
								codeplugContactGetRXGroup(contactListContactData.NOT_IN_CODEPLUGDATA_indexNumber))
						{
							menuContactListTimeout = 2000;
							menuContactListOverrideState = MENU_CONTACT_LIST_TG_IN_RXGROUP;
							voicePromptsAppendLanguageString(&currentLanguage->contact_used);
							voicePromptsAppendLanguageString(&currentLanguage->in_rx_group);
							voicePromptsPlay();
						}
						else
						{
							menuContactListOverrideState = MENU_CONTACT_LIST_CONFIRM;
							voicePromptsAppendLanguageString(&currentLanguage->delete_contact_qm);
							voicePromptsPlay();
						}
					}
					menuSystemPopPreviousMenu();
				}
				break;
		}
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		menuSystemMenuIncrement(&gMenusCurrentItemIndex, ((listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? NUM_CONTACT_LIST_QUICK_MENU_ITEMS : 1));
		updateSubMenuScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		menuSystemMenuDecrement(&gMenusCurrentItemIndex, ((listType == MENU_CONTACT_LIST_CONTACT_DIGITAL) ? NUM_CONTACT_LIST_QUICK_MENU_ITEMS : 1));
		updateSubMenuScreen();
	}
}

menuStatus_t menuContactListSubMenu(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		updateSubMenuScreen();
		keyboardInit();
		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		if (ev->hasEvent)
		{
			handleSubMenuEvent(ev);
		}
	}

	if (dtmfKeying)
	{
		if (!trxTransmissionEnabled)
		{
			// Start TX DTMF, prepare for ANALOG
			if (trxGetMode() != RADIO_MODE_ANALOG)
			{
				trxSetModeAndBandwidth(RADIO_MODE_ANALOG, false);
				trxSetTxCSS(CODEPLUG_CSS_NONE);
			}

			enableTransmission();

			trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_NONE);
			enableAudioAmp(AUDIO_AMP_MODE_RF);
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1);
			pretimePeriod = PITCounter + pretimeDuration;
			pretimeOver = false;
		}

		// DTMF has been TXed, restore DIGITAL/ANALOG
		if (dtmfpoLen == 0U)
		{
			disableTransmission();

			if (trxTransmissionEnabled)
			{
				// Stop TXing;
				trxTransmissionEnabled = false;
				trx_setRX();
				GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);

				trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_MIC);
				disableAudioAmp(AUDIO_AMP_MODE_RF);
				if (channelScreenChannelData.chMode == RADIO_MODE_ANALOG)
				{
					trxSetModeAndBandwidth(channelScreenChannelData.chMode, ((channelScreenChannelData.flag4 & 0x02) == 0x02));
					trxSetTxCSS(channelScreenChannelData.txTone);
				}
				else
				{
					trxSetModeAndBandwidth(channelScreenChannelData.chMode, false);// bandwidth false = 12.5Khz as DMR uses 12.5kHz
					trxSetDMRColourCode(channelScreenChannelData.txColor);
				}
			}

			dtmfKeying = false;
			menuSystemPopPreviousMenu();
		}

		if (dtmfpoLen > 0U)
		{
			dtmfProcess();
		}
	}

	return MENU_STATUS_SUCCESS;
}

void menuContactListDTMFSequenceReset(void)
{
	dtmfpoLen = 0U;
	dtmfpoPtr = 0U;
	dtmfKeying = false;
}

bool menuContactListIsDTMFSequenceKeying(void)
{
	return dtmfKeying;
}

static void dtmfPrepareSequence(uint8_t *seq)
{
	uint8_t len = 16U;

	menuContactListDTMFSequenceReset();

	memcpy(dtmfBuffer, seq, 16);
	dtmfBuffer[16] = 0xFFU;

	// non empty
	if (dtmfBuffer[0] != 0xFFU)
	{
		// Find the sequence length
		for (uint8_t i = 0; i < 16; i++)
		{
			if (dtmfBuffer[i] == 0xFFU)
			{
				len = i;
				break;
			}
		}

		dtmfpoLen = len;
		dtmfKeying = (len > 0);
	}
}

static void dtmfProcess(void)
{
	if (dtmfpoLen == 0U)
	{
		return;
	}

	if (!pretimeOver)
	{
		if (PITCounter > pretimePeriod)
		{
			pretimeOver = true;
		}
	}
	else
	{
		if (PITCounter > dtmfNextPeriod)
		{
			dtmfPausePeriod = PITCounter + dtmfDuration;
			dtmfNextPeriod = dtmfPausePeriod + dtmfPauseDuration;
			dtmfPaused = false;

			if (dtmfBuffer[dtmfpoPtr] != 0xFFU)
			{
				trxSetDTMF(dtmfBuffer[dtmfpoPtr]);
				trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_DTMF);
			}

			dtmfpoPtr++;

			if (dtmfpoPtr > dtmfpoLen)
			{
				dtmfpoPtr = 0U;
				dtmfpoLen = 0U;
			}
		}
		else if (( PITCounter > dtmfPausePeriod) && !dtmfPaused )
		{
			trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_NONE);
			dtmfPaused = true;
		}
	}
}
