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
#ifndef _VOICE_PROMPTS_H_
#define _VOICE_PROMPTS_H_

enum voicePrompts { PROMPT_SILENCE = 0, PROMPT_POINT, PROMPT_0, PROMPT_1, PROMPT_2, PROMPT_3, PROMPT_4, PROMPT_5, PROMPT_6, PROMPT_7, PROMPT_8, PROMPT_9,
	PROMPT_A, PROMPT_B, PROMPT_C, PROMPT_D, PROMPT_E, PROMPT_F, PROMPT_G, PROMPT_H, PROMPT_I, PROMPT_J, PROMPT_K, PROMPT_L, PROMPT_M,
	PROMPT_N, PROMPT_O, PROMPT_P, PROMPT_Q, PROMPT_R, PROMPT_S, PROMPT_T, PROMPT_U, PROMPT_V, PROMPT_W, PROMPT_X, PROMPT_Y, PROMPT_Z,
	PROMPT_Z2,
	PROMPT_CHANNEL, PROMPT_CONTACT, PROMPT_DBM, PROMPT_ENTRY, PROMPT_MEGAHERTZ, PROMPT_KILOHERTZ, PROMPT_TALKGROUP, PROMPT_TIMESLOT,
	PROMPT_VFO, PROMPT_SECONDS, PROMPT_MINUTES, PROMPT_HOURS, PROMPT_VOLTS, PROMPT_WATT, PROMPT_WATTS, PROMPT_MILLIWATTS, PROMPT_PERCENT,
	PROMPT_RECEIVE, PROMPT_TRANSMIT, PROMPT_HASH, PROMPT_STAR, PROMPT_GREEN, PROMPT_RED, PROMPT_VERSION, PROMPT_MODE, PROMPT_DMR, PROMPT_FM, PROMPT_WIDE, PROMPT_NARROW,
	PROMPT_PLUS, PROMPT_MINUS,
	PROMPT_VFO_EXCHANGE_TX_RX, PROMPT_VFO_COPY_RX_TO_TX, PROMPT_VFO_COPY_TX_TO_RX,
	PROMPT_POWER, PROMPT_CHANNEL_MODE, PROMPT_SCAN_MODE, PROMPT_TIMESLOT_MODE, PROMPT_FILTER_MODE, PROMPT_ZONE_MODE, PROMPT_POWER_MODE, PROMPT_COLORCODE_MODE,
	PROMPT_TBD1, PROMPT_TBD2, PROMPT_TBD3, PROMPT_TBD4, PROMPT_TBD5, PROMPT_TBD6, PROMPT_TBD7, PROMPT_TBD8, PROMPT_TBD9, PROMPT_TBD10,
	PROMPT_TBD11, PROMPT_TBD12, PROMPT_TBD13, PROMPT_TBD14, PROMPT_TBD15, PROMPT_TBD16, PROMPT_TBD17, PROMPT_TBD18, PROMPT_TBD19, PROMPT_TBD20,
	NUM_VOICE_PROMPTS
};


extern bool voicePromptIsActive;
extern bool voicePromptDataIsLoaded;

void voicePromptsCacheInit(void);
void voicePromptsTick(void);// Called from HR-C6000.c

void voicePromptsInit(void);// Call before building the prompt sequence
void voicePromptsAppendPrompt(uint8_t prompt);// Append an individual prompt item. This can be a single letter number or a phrase
void voicePromptsAppendString(char *);// Append a text string e.g. "VK3KYY"
void voicePromptsAppendInteger(int32_t value); // Append a signed integer
void voicePromptsAppendLanguageString(const char * const *);//Append a text from the current language e.g. &currentLanguage->battery
void voicePromptsPlay(void);// Starts prompt playback
bool voicePromptsIsPlaying(void);
bool voicePromptsHasDataToPlay(void);
void voicePromptsTerminate(void);

#endif
