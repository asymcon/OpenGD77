/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
 * 				and	 Colin Durbridge, G4EML
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
#include <settings.h>
#include <trx.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiUtilities.h>
#include <user_interface/uiLocalisation.h>

static void updateScreen(bool isFirstRun, bool allowedToSpeakUpdate);
static void updateCursor(bool moved);
static void handleEvent(uiEvent_t *ev);

static int32_t RxCSSIndex = 0;
static int32_t TxCSSIndex = 0;
static CSSTypes_t RxCSSType = CSS_NONE;
static CSSTypes_t TxCSSType = CSS_NONE;
static struct_codeplugChannel_t tmpChannel;// update a temporary copy of the channel and only write back if green menu is pressed
static char channelName[17];
static int namePos;

static menuStatus_t menuChannelDetailsExitCode = MENU_STATUS_SUCCESS;


enum CHANNEL_DETAILS_DISPLAY_LIST {  CH_DETAILS_RXCTCSS = 0, 
									 CH_DETAILS_TXCTCSS, CH_DETAILS_BANDWIDTH, 
									 CH_DETAILS_FREQ_STEP, CH_DETAILS_MODE, 
									 CH_DETAILS_RXGROUP, CH_DETAILS_DMR_CC,
									 CH_DETAILS_DMR_TS, CH_DETAILS_ZONE_SKIP,
									 CH_DETAILS_ALL_SKIP, CH_DETAILS_TOT,
									 CH_DETAILS_NAME, CH_DETAILS_RXFREQ,
									 CH_DETAILS_TXFREQ, CH_DETAILS_VOX,
									NUM_CH_DETAILS_ITEMS};// The last item in the list is used so that we automatically get a total number of items in the list

// Returns the index in either the CTCSS or DCS list of the tone (or closest match)
static int cssIndex(uint16_t tone, CSSTypes_t type)
{
	switch (type)
	{
		case CSS_CTCSS:
			for (int i = 0; i < TRX_NUM_CTCSS; i++)
			{
				if (TRX_CTCSSTones[i] >= tone)
				{
					return i;
				}
			}
			break;
		case CSS_DCS:
		case CSS_DCS_INVERTED:
			tone &= 0777;
			for (int i = 0; i < TRX_NUM_DCS; i++)
			{
				if (TRX_DCSCodes[i] >= tone)
				{
					return i;
				}
			}
			break;
		case CSS_NONE:
			break;
	}
	return 0;
}

uint16_t cssGetTone(int32_t index, CSSTypes_t type)
{
	if (index >= 0)
	{
		switch (type)
		{
			case CSS_CTCSS:
				if (index < TRX_NUM_CTCSS)
				{
					return TRX_CTCSSTones[index];
				}
				break;
			case CSS_DCS:
				if (index < TRX_NUM_DCS)
				{
					return (TRX_DCSCodes[index] | 0x8000);
				}
				break;
			case CSS_DCS_INVERTED:
				if (index < TRX_NUM_DCS)
				{
					return (TRX_DCSCodes[index] | 0xC000);
				}
				break;
			case CSS_NONE:
				break;
		}
	}
	return TRX_CTCSSTones[0];
}

void cssIncrement(uint16_t *tone, int32_t *index, CSSTypes_t *type, bool loop, bool stayInCSSType)
{
	(*index)++;
	switch (*type)
	{
		case CSS_CTCSS:
			if (*index >= TRX_NUM_CTCSS)
			{
				if (stayInCSSType)
				{
					*index = 0;
				}
				else
				{
					*type = CSS_DCS;
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0x8000;
					return;
				}
			}
			*tone = TRX_CTCSSTones[*index];
			break;
		case CSS_DCS:
			if (*index >= TRX_NUM_DCS)
			{
				if (stayInCSSType)
				{
					*index = 0;
				}
				else
				{
					*type = CSS_DCS_INVERTED;
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0xC000;
					return;
				}
			}
			*tone = TRX_DCSCodes[*index] | 0x8000;
			break;
		case CSS_DCS_INVERTED:
			if (*index >= TRX_NUM_DCS)
			{
				if (stayInCSSType)
				{
					*index = 0;
				}
				else
				{
					if (loop)
					{
						*type = CSS_CTCSS;
						*index = 0;
						*tone = TRX_CTCSSTones[*index];
						return;
					}
					*index = TRX_NUM_DCS - 1;
				}
			}
			*tone = TRX_DCSCodes[*index] | 0xC000;
			break;
		case CSS_NONE:
			*type = CSS_CTCSS;
			*index = 0;
			*tone = TRX_CTCSSTones[*index];
			break;
	}
	return;
}

static void cssIncrementFromEvent(uiEvent_t *ev, uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	if (BUTTONCHECK_DOWN(ev, BUTTON_SK2))
	{
		switch (*type)
		{
			case CSS_CTCSS:
				if (*index < (TRX_NUM_CTCSS - 1))
				{
					*index = (TRX_NUM_CTCSS - 1);
					*tone = TRX_CTCSSTones[*index];
				}
				else
				{
					*type = CSS_DCS;
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				break;
			case CSS_DCS:
				if (*index < (TRX_NUM_DCS - 1))
				{
					*index = (TRX_NUM_DCS - 1);
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				else
				{
					*type = CSS_DCS_INVERTED;
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0xC000;
				}
				break;
			case CSS_DCS_INVERTED:
				if (*index < (TRX_NUM_DCS - 1))
				{
					*index = (TRX_NUM_DCS - 1);
					*tone = TRX_DCSCodes[*index] | 0xC000;
				}
				break;
			case CSS_NONE:
				*type = CSS_CTCSS;
				*index = 0;
				*tone = TRX_CTCSSTones[*index];
				break;
		}
	}
	else
	{
		// Step +5, cssIncrement() handles index overflow
		if (ev->keys.event & KEY_MOD_LONG)
		{
			*index += 4;
		}
		cssIncrement(tone, index, type, false, false);
	}
}

static void cssDecrement(uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	(*index)--;
	switch (*type)
	{
		case CSS_CTCSS:
			if (*index < 0)
			{
				*type = CSS_NONE;
				*index = 0;
				*tone = CODEPLUG_CSS_NONE;
				return;
			}
			*tone = TRX_CTCSSTones[*index];
			break;
		case CSS_DCS:
			if (*index < 0)
			{
				*type = CSS_CTCSS;
				*index = TRX_NUM_CTCSS - 1;
				*tone = TRX_CTCSSTones[*index];
				return;
			}
			*tone = TRX_DCSCodes[*index] | 0x8000;
			break;
		case CSS_DCS_INVERTED:
			if (*index < 0)
			{
				*type = CSS_DCS;
				*index = (TRX_NUM_DCS - 1);
				*tone = TRX_DCSCodes[*index] | 0x8000;
				return;
			}
			*tone = TRX_DCSCodes[*index] | 0xC000;
			break;
		case CSS_NONE:
			*index = 0;
			*tone = CODEPLUG_CSS_NONE;
			break;
	}
}

static void cssDecrementFromEvent(uiEvent_t *ev, uint16_t *tone, int32_t *index, CSSTypes_t *type)
{
	if (BUTTONCHECK_DOWN(ev, BUTTON_SK2))
	{
		switch (*type)
		{
			case CSS_CTCSS:
				if (*index > 0)
				{
					*index = 0;
					*tone = TRX_CTCSSTones[*index];
				}
				else
				{
					*type = CSS_NONE;
					*index = 0;
					*tone = CODEPLUG_CSS_NONE;
				}
				break;
			case CSS_DCS:
				if (*index > 0)
				{
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				else
				{
					*type = CSS_CTCSS;
					*index = (TRX_NUM_CTCSS - 1);
					*tone = TRX_CTCSSTones[*index];
				}
				break;
			case CSS_DCS_INVERTED:
				if (*index > 0)
				{
					*index = 0;
					*tone = TRX_DCSCodes[*index] | 0xC000;
				}
				else
				{
					*type = CSS_DCS;
					*index = (TRX_NUM_DCS - 1);
					*tone = TRX_DCSCodes[*index] | 0x8000;
				}
				break;
			case CSS_NONE:
				break;
		}
	}
	else
	{
		// Step -5, cssDecrement() handles index < 0
		if (ev->keys.event & KEY_MOD_LONG)
		{
			*index -= 2;
		}
		cssDecrement(tone, index, type);
	}
}

menuStatus_t menuChannelDetails(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		freq_enter_idx = 0;
		memcpy(&tmpChannel, currentChannelData,sizeof(struct_codeplugChannel_t));

		if (codeplugChannelToneIsDCS(tmpChannel.txTone))
		{
			if (tmpChannel.txTone & CODEPLUG_DCS_INVERTED_MASK)
			{
				TxCSSType = CSS_DCS_INVERTED;
			}
			else
			{
				TxCSSType = CSS_DCS;
			}
		}
		else if (codeplugChannelToneIsCTCSS(tmpChannel.txTone))
		{
			TxCSSType = CSS_CTCSS;
		}
		TxCSSIndex = cssIndex(tmpChannel.txTone, TxCSSType);

		if (codeplugChannelToneIsDCS(tmpChannel.rxTone))
		{
			if (tmpChannel.rxTone & CODEPLUG_DCS_INVERTED_MASK)
			{
				RxCSSType = CSS_DCS_INVERTED;
			}
			else
			{
				RxCSSType = CSS_DCS;
			}
		}
		else if (codeplugChannelToneIsCTCSS(tmpChannel.rxTone))
		{
			RxCSSType = CSS_CTCSS;
		}
		RxCSSIndex = cssIndex(tmpChannel.rxTone, RxCSSType);


		codeplugUtilConvertBufToString(tmpChannel.name, channelName, 16);
		namePos = strlen(channelName);

		if ((settingsCurrentChannelNumber == -1) && (namePos == 0)) // In VFO, and VFO has no name in the codeplug
		{
			snprintf(channelName, 17, "VFO %s", (nonVolatileSettings.currentVFONumber == 0 ? "A" : "B"));
			namePos = 5;
		}

		voicePromptsInit();
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendLanguageString(&currentLanguage->channel_details);
		voicePromptsAppendPrompt(PROMPT_SILENCE);
		voicePromptsAppendPrompt(PROMPT_SILENCE);

		updateScreen(true, true);
		updateCursor(true);

		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}
	else
	{
		menuChannelDetailsExitCode = MENU_STATUS_SUCCESS;

		updateCursor(false);

		if (ev->hasEvent)
		{
			handleEvent(ev);
		}
	}
	return menuChannelDetailsExitCode;
}

static void updateCursor(bool moved)
{
	if (settingsCurrentChannelNumber != -1)
	{
		switch (gMenusCurrentItemIndex)
		{
		case CH_DETAILS_NAME:
			menuUpdateCursor(namePos, moved, true);
			break;
		}
	}
}

static void updateScreen(bool isFirstRun, bool allowedToSpeakUpdate)
{
	int mNum = 0;
	static const int bufferLen = 17;
	char buf[bufferLen];
	int tmpVal;
	int val_before_dp;
	int val_after_dp;
	struct_codeplugRxGroup_t rxGroupBuf;
	char rxNameBuf[bufferLen];
	char * const *leftSide = NULL;// initialise to please the compiler
	char * const *rightSideConst = NULL;// initialise to please the compiler
	char rightSideVar[bufferLen];

	ucClearBuf();
	menuDisplayTitle(currentLanguage->channel_details);

	if (freq_enter_idx != 0)
	{
		snprintf(buf, bufferLen, "%c%c%c.%c%c%c%c%c MHz", freq_enter_digits[0], freq_enter_digits[1], freq_enter_digits[2],
				freq_enter_digits[3], freq_enter_digits[4], freq_enter_digits[5], freq_enter_digits[6], freq_enter_digits[7]);
		ucPrintCentered(32, buf, FONT_SIZE_3);
	}
	else
	{
		keypadAlphaEnable = (gMenusCurrentItemIndex == CH_DETAILS_NAME);

		// Can only display 3 of the options at a time menu at -1, 0 and +1
		for(int i = -1; i <= 1; i++)
		{
			mNum = menuGetMenuOffset(NUM_CH_DETAILS_ITEMS, i);
			buf[0] = 0;

			rightSideVar[0] = 0;
			switch(mNum)
			{
				case CH_DETAILS_NAME:
					strncpy(rightSideVar, channelName, 17);
					leftSide = NULL;
				break;
				case CH_DETAILS_MODE:
					leftSide = (char * const *)&currentLanguage->mode;
					strcpy(rightSideVar,(tmpChannel.chMode == RADIO_MODE_ANALOG)?"FM":"DMR");
					break;
				break;
				case CH_DETAILS_DMR_CC:
					leftSide = (char * const *)&currentLanguage->colour_code;
					rightSideConst = (char * const *)&currentLanguage->n_a;
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						rightSideConst = (char * const *)&currentLanguage->n_a;
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "%d", tmpChannel.txColor);
					}
					break;
				case CH_DETAILS_DMR_TS:
					leftSide = (char * const *)&currentLanguage->timeSlot;
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						rightSideConst = (char * const *)&currentLanguage->n_a;
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "%d", ((tmpChannel.flag2 & 0x40) >> 6) + 1);
					}
					break;
				case CH_DETAILS_RXCTCSS:
					leftSide = NULL;
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						if (codeplugChannelToneIsCTCSS(tmpChannel.rxTone))
						{
							snprintf(rightSideVar, bufferLen, "Rx CTCSS:%d.%dHz", tmpChannel.rxTone / 10 , tmpChannel.rxTone % 10);
						}
						else if (codeplugChannelToneIsDCS(tmpChannel.rxTone))
						{
							snprintf(rightSideVar, bufferLen, "Rx DCS:D%03o%c", tmpChannel.rxTone & 0777, (tmpChannel.rxTone & CODEPLUG_DCS_INVERTED_MASK) ? 'I' : 'N');
						}
						else
						{
							snprintf(rightSideVar, bufferLen, "Rx CSS:%s", currentLanguage->none);
						}
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "Rx CSS:%s", currentLanguage->n_a);
					}
					break;
				case CH_DETAILS_TXCTCSS:
					leftSide = NULL;
					if (tmpChannel.chMode == RADIO_MODE_ANALOG)
					{
						if (codeplugChannelToneIsCTCSS(tmpChannel.txTone))
						{
							snprintf(rightSideVar, bufferLen, "Tx CTCSS:%d.%dHz", tmpChannel.txTone / 10 , tmpChannel.txTone % 10);
						}
						else if (codeplugChannelToneIsDCS(tmpChannel.txTone))
						{
							snprintf(rightSideVar, bufferLen, "Tx DCS:D%03o%c", tmpChannel.txTone & 0777, (tmpChannel.txTone & CODEPLUG_DCS_INVERTED_MASK) ? 'I' : 'N');
						}
						else
						{
							snprintf(rightSideVar, bufferLen, "Tx CSS:%s", currentLanguage->off);
						}
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "Tx CSS:%s", currentLanguage->n_a);
					}
					break;
				case CH_DETAILS_RXFREQ:
					leftSide = NULL;
					val_before_dp = tmpChannel.rxFreq / 100000;
					val_after_dp = tmpChannel.rxFreq - val_before_dp * 100000;
					snprintf(rightSideVar, bufferLen, "Rx:%d.%05dMHz", val_before_dp, val_after_dp);
					break;
				case CH_DETAILS_TXFREQ:
					leftSide = NULL;
					val_before_dp = tmpChannel.txFreq / 100000;
					val_after_dp = tmpChannel.txFreq - val_before_dp * 100000;
					snprintf(rightSideVar, bufferLen, "Tx:%d.%05dMHz", val_before_dp, val_after_dp);
					break;
				case CH_DETAILS_BANDWIDTH:
					// Bandwidth
					leftSide = (char * const *)&currentLanguage->bandwidth;
					if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
					{
						snprintf(rightSideVar, bufferLen, "%s" , currentLanguage->n_a);
					}
					else
					{
						snprintf(rightSideVar, bufferLen, "%s", ((tmpChannel.flag4 & 0x02) == 0x02) ? "25kHz" : "12.5kHz");
					}
					break;
				case CH_DETAILS_FREQ_STEP:
					leftSide = (char * const *)&currentLanguage->stepFreq;
					tmpVal = VFO_FREQ_STEP_TABLE[(tmpChannel.VFOflag5 >> 4)] / 100;
					snprintf(rightSideVar, bufferLen, "%d.%02dkHz",  tmpVal, VFO_FREQ_STEP_TABLE[(tmpChannel.VFOflag5 >> 4)] - (tmpVal * 100));
					break;
				case CH_DETAILS_TOT:// TOT
					leftSide = (char * const *)&currentLanguage->tot;
					if (tmpChannel.tot != 0)
					{
						snprintf(rightSideVar, bufferLen, "%dS",tmpChannel.tot * 5);
					}
					else
					{
						rightSideConst = (char * const *)&currentLanguage->off;
					}
					break;
				case CH_DETAILS_ZONE_SKIP:						// Zone Scan Skip Channel (Using CPS Auto Scan flag)
					leftSide = (char * const *)&currentLanguage->zone_skip;
					rightSideConst = (char * const *)(((tmpChannel.flag4 & 0x20) == 0x20) ? &currentLanguage->yes : &currentLanguage->no);
					break;
				case CH_DETAILS_ALL_SKIP:					// All Scan Skip Channel (Using CPS Lone Worker flag)
					leftSide = (char * const *)&currentLanguage->all_skip;
					rightSideConst = (char * const *)(((tmpChannel.flag4 & 0x10) == 0x10) ? &currentLanguage->yes : &currentLanguage->no);
					break;

				case CH_DETAILS_RXGROUP:
					leftSide = (char * const *)&currentLanguage->rx_group;
					if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
					{
						codeplugRxGroupGetDataForIndex(tmpChannel.rxGroupList, &rxGroupBuf);
						codeplugUtilConvertBufToString(rxGroupBuf.name, rxNameBuf, 16);
						snprintf(rightSideVar, bufferLen, "%s", rxNameBuf);
					}
					else
					{
						rightSideConst = (char * const *)&currentLanguage->n_a;
					}
					break;
				case CH_DETAILS_VOX:
					leftSide = NULL;
					snprintf(rightSideVar, bufferLen, "VOX:%s", ((tmpChannel.flag4 & 0x40) == 0x40) ? currentLanguage->on : currentLanguage->off);
					break;
			}

			if (leftSide != NULL)
			{
				snprintf(buf, bufferLen, "%s:%s", *leftSide, (rightSideVar[0] ? rightSideVar : *rightSideConst));
			}
			else
			{
				strcpy(buf, rightSideVar);
			}

			if ((i == 0) && (allowedToSpeakUpdate && (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1)))
			{
				if (!isFirstRun)
				{
					voicePromptsInit();
				}
				if (leftSide != NULL)
				{
					voicePromptsAppendLanguageString((const char * const *)leftSide);
				}

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
	}
	ucRender();
	displayLightTrigger();
}

static void updateFrequency(void)
{
	int tmp_frequency = read_freq_enter_digits(0, 8);

	if (trxGetBandFromFrequency(tmp_frequency) != -1)
	{
		switch (gMenusCurrentItemIndex)
		{
			case CH_DETAILS_RXFREQ:
				tmpChannel.rxFreq = tmp_frequency;
				break;
			case CH_DETAILS_TXFREQ:
				tmpChannel.txFreq = tmp_frequency;
				break;
		}
		reset_freq_enter_digits();
	}
	else
	{
		soundSetMelody(MELODY_ERROR_BEEP);
	}
}

static void handleEvent(uiEvent_t *ev)
{
	int tmpVal;
	struct_codeplugRxGroup_t rxGroupBuf;

	if ((ev->function > 0) && (ev->function < NUM_CH_DETAILS_ITEMS))
	{
		gMenusCurrentItemIndex = ev->function;
	}

	if (ev->events & BUTTON_EVENT)
	{
		if (repeatVoicePromptOnSK1(ev))
		{
			return;
		}
	}

	if ((gMenusCurrentItemIndex == CH_DETAILS_RXFREQ) || (gMenusCurrentItemIndex == CH_DETAILS_TXFREQ))
	{
		if (freq_enter_idx != 0)
		{
			if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
			{
				updateFrequency();
				updateScreen(false, true);
				return;
			}
			if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
			{
				updateScreen(false, true);
				return;
			}
			if (KEYCHECK_SHORTUP(ev->keys, KEY_LEFT))
			{
				freq_enter_idx--;
				freq_enter_digits[freq_enter_idx] = '-';
				updateScreen(false, true);
				return;
			}
		}

		if (freq_enter_idx < 8)
		{
			int keyval = menuGetKeypadKeyValue(ev, true);
			if (keyval != 99)
			{
				freq_enter_digits[freq_enter_idx] = (char) keyval + '0';
				freq_enter_idx++;

				if (freq_enter_idx == 8)
				{
					updateFrequency();
					freq_enter_idx = 0;
				}
				updateScreen(false, true);
				return;
			}
		}
	}

	// Not entering a frequency numeric digit
	bool allowedToSpeakUpdate = true;
	if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		menuSystemMenuIncrement(&gMenusCurrentItemIndex, NUM_CH_DETAILS_ITEMS);
		updateScreen(false, true);
		menuChannelDetailsExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		menuSystemMenuDecrement(&gMenusCurrentItemIndex, NUM_CH_DETAILS_ITEMS);
		updateScreen(false, true);
		menuChannelDetailsExitCode |= MENU_STATUS_LIST_TYPE;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_RIGHT))
	{
		switch(gMenusCurrentItemIndex)
		{
			case CH_DETAILS_NAME:
				if (settingsCurrentChannelNumber != -1)
				{
					moveCursorRightInString(channelName, &namePos, 16, BUTTONCHECK_DOWN(ev, BUTTON_SK2));
					updateCursor(true);
					allowedToSpeakUpdate = false;
				}
				break;
			case CH_DETAILS_MODE:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpChannel.chMode = RADIO_MODE_ANALOG;
				}
				break;
			case CH_DETAILS_DMR_CC:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					if (tmpChannel.txColor < 15)
					{
						tmpChannel.txColor++;
						trxSetDMRColourCode(tmpChannel.txColor);
					}
				}
				break;
			case CH_DETAILS_DMR_TS:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpChannel.flag2 |= 0x40;// set TS 2 bit
				}
				break;
			case CH_DETAILS_RXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssIncrementFromEvent(ev, &tmpChannel.rxTone, &RxCSSIndex, &RxCSSType);
					trxSetRxCSS(tmpChannel.rxTone);
					announceCSSCode(tmpChannel.rxTone, RxCSSType, (tmpChannel.rxTone & CODEPLUG_DCS_INVERTED_MASK));
					allowedToSpeakUpdate = false;
				}
				break;
			case CH_DETAILS_TXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssIncrementFromEvent(ev, &tmpChannel.txTone, &TxCSSIndex, &TxCSSType);
					announceCSSCode(tmpChannel.txTone, TxCSSType, (tmpChannel.txTone & CODEPLUG_DCS_INVERTED_MASK));
					allowedToSpeakUpdate = false;
				}
				break;
			case CH_DETAILS_BANDWIDTH:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					tmpChannel.flag4 |= 0x02;// set 25kHz bit
				}
				break;
			case CH_DETAILS_FREQ_STEP:
				tmpVal = (tmpChannel.VFOflag5 >> 4) + 1;
				if (tmpVal > 7)
				{
					tmpVal = 7;
				}
				tmpChannel.VFOflag5 &= 0x0F;
				tmpChannel.VFOflag5 |= tmpVal << 4;
				break;
			case CH_DETAILS_TOT:
				if (tmpChannel.tot < 255)
				{
					tmpChannel.tot++;
				}
				break;
			case CH_DETAILS_ZONE_SKIP:
				tmpChannel.flag4 |= 0x20;// set Channel Zone Skip bit (was Auto Scan)
				break;
			case CH_DETAILS_ALL_SKIP:
				tmpChannel.flag4 |= 0x10;// set Channel All Skip bit (was Lone Worker)
				break;
			case CH_DETAILS_RXGROUP:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpVal = tmpChannel.rxGroupList;
					tmpVal++;
					while (tmpVal < 76)
					{
						if (codeplugRxGroupGetDataForIndex(tmpVal, &rxGroupBuf))
						{
							tmpChannel.rxGroupList = tmpVal;
							break;
						}
						tmpVal++;
					}
				}
				break;
			case CH_DETAILS_VOX:
				tmpChannel.flag4 |= 0x40;
				break;
		}
		updateScreen(false, allowedToSpeakUpdate);
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_LEFT))
	{
		switch(gMenusCurrentItemIndex)
		{
			case CH_DETAILS_NAME:
				if (settingsCurrentChannelNumber != -1)
				{
					moveCursorLeftInString(channelName, &namePos, BUTTONCHECK_DOWN(ev, BUTTON_SK2));
					updateCursor(true);
					allowedToSpeakUpdate = false;
				}
				break;
			case CH_DETAILS_MODE:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					tmpChannel.chMode = RADIO_MODE_DIGITAL;
					tmpChannel.flag4 &= ~0x02;// clear 25kHz bit
				}
				break;
			case CH_DETAILS_DMR_CC:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					if (tmpChannel.txColor > 0)
					{
						tmpChannel.txColor--;
						trxSetDMRColourCode(tmpChannel.txColor);
					}
				}
				break;
			case CH_DETAILS_DMR_TS:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpChannel.flag2 &= 0xBF;// Clear TS 2 bit
				}
				break;
			case CH_DETAILS_RXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssDecrementFromEvent(ev, &tmpChannel.rxTone, &RxCSSIndex, &RxCSSType);
					trxSetRxCSS(tmpChannel.rxTone);
					announceCSSCode(tmpChannel.rxTone, RxCSSType, (tmpChannel.rxTone & CODEPLUG_DCS_INVERTED_MASK));
					allowedToSpeakUpdate = false;
				}
				break;
			case CH_DETAILS_TXCTCSS:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					cssDecrementFromEvent(ev, &tmpChannel.txTone, &TxCSSIndex, &TxCSSType);
					announceCSSCode(tmpChannel.txTone, TxCSSType, (tmpChannel.txTone & CODEPLUG_DCS_INVERTED_MASK));
					allowedToSpeakUpdate = false;
				}
				break;
			case CH_DETAILS_BANDWIDTH:
				if (tmpChannel.chMode == RADIO_MODE_ANALOG)
				{
					tmpChannel.flag4 &= ~0x02;// clear 25kHz bit
				}
				break;
			case CH_DETAILS_FREQ_STEP:
				tmpVal = (tmpChannel.VFOflag5 >> 4) - 1;
				if (tmpVal < 0)
				{
					tmpVal = 0;
				}
				tmpChannel.VFOflag5 &= 0x0F;
				tmpChannel.VFOflag5 |= tmpVal << 4;
				break;
			case CH_DETAILS_TOT:
				if (tmpChannel.tot > 0)
				{
					tmpChannel.tot--;
				}
				break;
			case CH_DETAILS_ZONE_SKIP:
				tmpChannel.flag4 &= ~0x20;// clear Channel Zone Skip Bit (was Auto Scan bit)
				break;
			case CH_DETAILS_ALL_SKIP:
				tmpChannel.flag4 &= ~0x10;// clear Channel All Skip Bit (was Lone Worker bit)
				break;
			case CH_DETAILS_RXGROUP:
				if (tmpChannel.chMode == RADIO_MODE_DIGITAL)
				{
					tmpVal = tmpChannel.rxGroupList;
					tmpVal--;
					while (tmpVal > 0)
					{
						if (codeplugRxGroupGetDataForIndex(tmpVal, &rxGroupBuf))
						{
							tmpChannel.rxGroupList = tmpVal;
							break;
						}
						tmpVal--;
					}
				}
				break;
			case CH_DETAILS_VOX:
				tmpChannel.flag4 &= ~0x40;
				break;

		}
		updateScreen(false, allowedToSpeakUpdate);
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		if (settingsCurrentChannelNumber != -1)
		{
			codeplugUtilConvertStringToBuf(channelName, (char *)&tmpChannel.name, 16);
		}
		memcpy(currentChannelData, &tmpChannel, sizeof(struct_codeplugChannel_t));

		// settingsCurrentChannelNumber is -1 when in VFO mode
		// But the VFO is stored in the nonVolatile settings, and not saved back to the codeplug
		// Also don't store this back to the codeplug unless the Function key (Blue / SK2 ) is pressed at the same time.
		if ((settingsCurrentChannelNumber != -1) && BUTTONCHECK_DOWN(ev, BUTTON_SK2))
		{
			codeplugChannelSaveDataForIndex(settingsCurrentChannelNumber, currentChannelData);
		}

		settingsSetVFODirty();
		settingsSaveIfNeeded(true);

		menuSystemPopAllAndDisplayRootMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if ((gMenusCurrentItemIndex == CH_DETAILS_NAME) && (settingsCurrentChannelNumber != -1))
	{
		if ((ev->keys.event == KEY_MOD_PREVIEW) && (namePos < 16))
		{
			channelName[namePos] = ev->keys.key;
			updateCursor(true);
			announceChar(ev->keys.key);
			updateScreen(false, false);
		}
		if ((ev->keys.event == KEY_MOD_PRESS) && (namePos < 16))
		{
			channelName[namePos] = ev->keys.key;
			if ((namePos < strlen(channelName)) && (namePos < 15))
			{
				namePos++;
			}
			updateCursor(true);
			announceChar(ev->keys.key);
			updateScreen(false, false);
		}
	}
}
