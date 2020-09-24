/*
 * Copyright (C)2019 	Kai Ludwig, DG4KLU
 * 				and		Roger Clark, VK3KYY / G4KYF
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
#include <EEPROM.h>
#include <settings.h>
#include <sound.h>
#include <trx.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiLocalisation.h>
#include <ticks.h>

static const int STORAGE_BASE_ADDRESS 		= 0x6000;

static const int STORAGE_MAGIC_NUMBER 		= 0x6022;

// Bit patterns for DMR Beep
const uint8_t BEEP_TX_NONE  = 0x00;
const uint8_t BEEP_TX_START = 0x01;
const uint8_t BEEP_TX_STOP  = 0x02;

#if defined(PLATFORM_RD5R)
static uint32_t dirtyTime = 0;
#endif

static bool settingsDirty = false;
static bool settingsVFODirty = false;
settingsStruct_t nonVolatileSettings;
struct_codeplugChannel_t *currentChannelData;
struct_codeplugChannel_t channelScreenChannelData = { .rxFreq = 0 };
struct_codeplugContact_t contactListContactData;
struct_codeplugChannel_t settingsVFOChannel[2];// VFO A and VFO B from the codeplug.
int contactListContactIndex;
int settingsUsbMode = USB_MODE_CPS;
int settingsCurrentChannelNumber = 0;

int *nextKeyBeepMelody = (int *)MELODY_KEY_BEEP;

bool settingsSaveSettings(bool includeVFOs)
{
	if (includeVFOs)
	{
		codeplugSetVFO_ChannelData(&settingsVFOChannel[CHANNEL_VFO_A], CHANNEL_VFO_A);
		codeplugSetVFO_ChannelData(&settingsVFOChannel[CHANNEL_VFO_B], CHANNEL_VFO_B);
		settingsVFODirty = false;
	}

	// Never reset this setting (as voicePromptsCacheInit() can change it if voice data are missing)
#if defined(PLATFORM_GD77S)
	nonVolatileSettings.audioPromptMode = AUDIO_PROMPT_MODE_VOICE_LEVEL_3;
#endif

	bool ret = EEPROM_Write(STORAGE_BASE_ADDRESS, (uint8_t*)&nonVolatileSettings, sizeof(settingsStruct_t));

	if (ret)
	{
		settingsDirty = false;
	}

	return ret;
}

bool settingsLoadSettings(void)
{
	bool readOK = EEPROM_Read(STORAGE_BASE_ADDRESS, (uint8_t*)&nonVolatileSettings, sizeof(settingsStruct_t));
	if ((nonVolatileSettings.magicNumber != STORAGE_MAGIC_NUMBER) || (readOK != true))
	{
		settingsRestoreDefaultSettings();
	}

// Force Hotspot mode to off for existing RD-5R users.
#if defined(PLATFORM_RD5R)
	nonVolatileSettings.hotspotType = HOTSPOT_TYPE_OFF;
#endif

	codeplugGetVFO_ChannelData(&settingsVFOChannel[CHANNEL_VFO_A], CHANNEL_VFO_A);
	codeplugGetVFO_ChannelData(&settingsVFOChannel[CHANNEL_VFO_B], CHANNEL_VFO_B);
	settingsInitVFOChannel(0);// clean up any problems with VFO data
	settingsInitVFOChannel(1);

	trxDMRID = codeplugGetUserDMRID();

	currentLanguage = &languages[nonVolatileSettings.languageIndex];

	soundBeepVolumeDivider = nonVolatileSettings.beepVolumeDivider;

	codeplugInitChannelsPerZone();// Initialise the codeplug channels per zone

	settingsDirty = false;
	settingsVFODirty = false;

	return readOK;
}

void settingsInitVFOChannel(int vfoNumber)
{
	// temporary hack in case the code plug has no RxGroup selected
	// The TG needs to come from the RxGroupList
	if (settingsVFOChannel[vfoNumber].rxGroupList == 0)
	{
		settingsVFOChannel[vfoNumber].rxGroupList = 1;
	}

	if (settingsVFOChannel[vfoNumber].contact == 0)
	{
		settingsVFOChannel[vfoNumber].contact = 1;
	}
}

void settingsRestoreDefaultSettings(void)
{
	nonVolatileSettings.magicNumber = STORAGE_MAGIC_NUMBER;
	nonVolatileSettings.currentChannelIndexInZone = 0;
	nonVolatileSettings.currentChannelIndexInAllZone = 1;
	nonVolatileSettings.currentIndexInTRxGroupList[SETTINGS_CHANNEL_MODE] = 0;
	nonVolatileSettings.currentIndexInTRxGroupList[SETTINGS_VFO_A_MODE] = 0;
	nonVolatileSettings.currentIndexInTRxGroupList[SETTINGS_VFO_B_MODE] = 0;
	nonVolatileSettings.currentZone = 0;
	nonVolatileSettings.backlightMode =
#if defined(PLATFORM_GD77S)
			BACKLIGHT_MODE_NONE;
#else
			BACKLIGHT_MODE_AUTO;
#endif
	nonVolatileSettings.backLightTimeout = 5;//0 = never timeout. 1 - 255 time in seconds
	nonVolatileSettings.displayContrast =
#if defined(PLATFORM_DM1801)
			0x0e; // 14
#elif defined (PLATFORM_RD5R)
			0x06;
#else
			0x12; // 18
#endif
	nonVolatileSettings.initialMenuNumber =
#if defined(PLATFORM_GD77S)
			UI_CHANNEL_MODE;
#else
			UI_VFO_MODE;
#endif
	nonVolatileSettings.displayBacklightPercentage = 5U;// 100% brightness
	nonVolatileSettings.displayBacklightPercentageOff = 0U;// 0% brightness
	nonVolatileSettings.displayInverseVideo = false;// Not inverse video
	nonVolatileSettings.useCalibration = true;// enable the new calibration system
	nonVolatileSettings.txFreqLimited = true;// Limit Tx frequency to US Amateur bands
//	#if(PLATFORM_GD-77  || PLATFORM_GD77S)
	nonVolatileSettings.txPowerLevel = 11;// 1W Power level in trx.c must match 1W level case
//	#elif (PLATFORM_DM1801)
//	nonVolatileSettings.txPowerLevel=4;// 1 W
//	#endif
	nonVolatileSettings.overrideTG = 0;// 0 = No override
	nonVolatileSettings.txTimeoutBeepX5Secs = 0;
	nonVolatileSettings.beepVolumeDivider =
#if defined(PLATFORM_GD77S)
			5; // -9dB: Beeps are way too loud on the GD77S
#else
			15; // no reduction in volume
#endif
	nonVolatileSettings.micGainDMR = 11; // Normal value used by the official firmware
	nonVolatileSettings.micGainFM = 17; // Default (from all of my cals, datasheet default: 16)
	nonVolatileSettings.tsManualOverride = 0; // No manual TS override using the Star key
	nonVolatileSettings.keypadTimerLong = 3;
	nonVolatileSettings.keypadTimerRepeat = 1;
	nonVolatileSettings.currentVFONumber = CHANNEL_VFO_A;
	nonVolatileSettings.dmrDestinationFilter =
#if defined(PLATFORM_GD77S)
	DMR_DESTINATION_FILTER_TG;
#else
	DMR_DESTINATION_FILTER_NONE;
#endif
	nonVolatileSettings.dmrCcTsFilter = DMR_CCTS_FILTER_CC_TS;


	nonVolatileSettings.dmrCaptureTimeout = 10;// Default to holding 10 seconds after a call ends
	nonVolatileSettings.analogFilterLevel = ANALOG_FILTER_CTCSS;
	nonVolatileSettings.languageIndex = 0;
	nonVolatileSettings.scanDelay = 5;// 5 seconds
	nonVolatileSettings.scanModePause = SCAN_MODE_HOLD;
	nonVolatileSettings.squelchDefaults[RADIO_BAND_VHF]		= 10;// 1 - 21 = 0 - 100% , same as from the CPS variable squelch
	nonVolatileSettings.squelchDefaults[RADIO_BAND_220MHz]	= 10;// 1 - 21 = 0 - 100% , same as from the CPS variable squelch
	nonVolatileSettings.squelchDefaults[RADIO_BAND_UHF]		= 10;// 1 - 21 = 0 - 100% , same as from the CPS variable squelch
	nonVolatileSettings.pttToggle = false; // PTT act as a toggle button
	nonVolatileSettings.hotspotType =
#if defined(PLATFORM_GD77S)
			HOTSPOT_TYPE_MMDVM;
#else
			HOTSPOT_TYPE_OFF;
#endif
	nonVolatileSettings.transmitTalkerAlias	= false;
    nonVolatileSettings.privateCalls =
#if defined(PLATFORM_GD77S)
    ALLOW_PRIVATE_CALLS_OFF;
#else
    ALLOW_PRIVATE_CALLS_ON;
#endif
    // Set all these value to zero to force the operator to set their own limits.
	nonVolatileSettings.vfoScanLow[CHANNEL_VFO_A] = 0;
	nonVolatileSettings.vfoScanLow[CHANNEL_VFO_B] = 0;
	nonVolatileSettings.vfoScanHigh[CHANNEL_VFO_A] = 0;
	nonVolatileSettings.vfoScanHigh[CHANNEL_VFO_B] = 0;


	nonVolatileSettings.contactDisplayPriority = CONTACT_DISPLAY_PRIO_CC_DB_TA;
	nonVolatileSettings.splitContact = SPLIT_CONTACT_SINGLE_LINE_ONLY;
	nonVolatileSettings.beepOptions =
#if defined(PLATFORM_GD77S)
			BEEP_TX_STOP |
#endif
			BEEP_TX_START;
	// VOX related
	nonVolatileSettings.voxThreshold = 20;
	nonVolatileSettings.voxTailUnits = 4; // 2 seconds tail
	nonVolatileSettings.audioPromptMode =
#if defined(PLATFORM_GD77S)
			AUDIO_PROMPT_MODE_VOICE_LEVEL_3;
#else
			AUDIO_PROMPT_MODE_NORMAL;
#endif

	currentChannelData = &settingsVFOChannel[nonVolatileSettings.currentVFONumber];// Set the current channel data to point to the VFO data since the default screen will be the VFO

	settingsDirty = true;

	settingsSaveSettings(false);
}

void settingsEraseCustomContent(void)
{
	//Erase OpenGD77 custom content
	SPI_Flash_eraseSector(0);// The first sector (4k) contains the OpenGD77 custom codeplug content e.g. Boot melody and boot image.
}

// --- Helpers ---
void settingsSetBOOL(bool *s, bool v)
{
	*s = v;
	settingsSetDirty();
}

void settingsSetINT8(int8_t *s, int8_t v)
{
	*s = v;
	settingsSetDirty();
}

void settingsSetUINT8(uint8_t *s, uint8_t v)
{
	*s = v;
	settingsSetDirty();
}

void settingsSetINT16(int16_t *s, int16_t v)
{
	*s = v;
	settingsSetDirty();
}

void settingsSetUINT16(uint16_t *s, uint16_t v)
{
	*s = v;
	settingsSetDirty();
}

void settingsSetINT32(int32_t *s, int32_t v)
{
	*s = v;
	settingsSetDirty();
}

void settingsSetUINT32(uint32_t *s, uint32_t v)
{
	*s = v;
	settingsSetDirty();
}

void settingsIncINT8(int8_t *s, int8_t v)
{
	*s = *s + v;
	settingsSetDirty();
}

void settingsIncUINT8(uint8_t *s, uint8_t v)
{
	*s = *s + v;
	settingsSetDirty();
}

void settingsIncINT16(int16_t *s, int16_t v)
{
	*s = *s + v;
	settingsSetDirty();
}

void settingsIncUINT16(uint16_t *s, uint16_t v)
{
	*s = *s + v;
	settingsSetDirty();
}

void settingsIncINT32(int32_t *s, int32_t v)
{
	*s = *s + v;
	settingsSetDirty();
}

void settingsIncUINT32(uint32_t *s, uint32_t v)
{
	*s = *s + v;
	settingsSetDirty();
}

void settingsDecINT8(int8_t *s, int8_t v)
{
	*s = *s - v;
	settingsSetDirty();
}

void settingsDecUINT8(uint8_t *s, uint8_t v)
{
	*s = *s - v;
	settingsSetDirty();
}

void settingsDecINT16(int16_t *s, int16_t v)
{
	*s = *s - v;
	settingsSetDirty();
}

void settingsDecUINT16(uint16_t *s, uint16_t v)
{
	*s = *s - v;
	settingsSetDirty();
}

void settingsDecINT32(int32_t *s, int32_t v)
{
	*s = *s - v;
	settingsSetDirty();
}

void settingsDecUINT32(uint32_t *s, uint32_t v)
{
	*s = *s - v;
	settingsSetDirty();
}
// --- End of Helpers ---


void settingsSetDirty(void)
{
	settingsDirty = true;

#if defined(PLATFORM_RD5R)
	dirtyTime = fw_millis();
#endif
}

void settingsSetVFODirty(void)
{
	settingsVFODirty = true;

#if defined(PLATFORM_RD5R)
	dirtyTime = fw_millis();
#endif
}

void settingsSaveIfNeeded(bool immediately)
{
#if defined(PLATFORM_RD5R)

	const int DIRTY_DURTION_MILLISECS = 500;

	if ((settingsDirty || settingsVFODirty) && (immediately || ((fw_millis() - dirtyTime) > DIRTY_DURTION_MILLISECS))) // DIRTY_DURTION_ has passed since last change
	{
		settingsSaveSettings(settingsVFODirty);
	}
#endif
}
