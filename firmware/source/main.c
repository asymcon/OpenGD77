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

#include <codeplug.h>
#include <main.h>
#include <settings.h>
#include <ticks.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiUtilities.h>
#include <user_interface/uiLocalisation.h>
#include <functions/voicePrompts.h>


#if defined(USE_SEGGER_RTT)
#include <SeggerRTT/RTT/SEGGER_RTT.h>
#endif

#ifdef NDEBUG
#error A firmware compiled in Release mode will not work, yet
#error Change target build to Debug then Clean the build and recompile
#endif

//#define READ_CPUID


bool PTTToggledDown = false; // PTT toggle feature

void mainTask(void *data);
#if defined(READ_CPUID)
void debugReadCPUID(void);
#endif

TaskHandle_t mainTaskHandle;


static uint32_t lowbatteryTimer = 0;
static const int LOW_BATTERY_INTERVAL = ((1000 * 60) * 1); // 1 minute;
const int LOW_BATTERY_WARNING_VOLTAGE_DIFFERENTIAL = 6;	// Offset between the minimum voltage and when the battery warning audio starts. 6 = 0.6V

void mainTaskInit(void)
{
	xTaskCreate(mainTask,                    /* pointer to the task */
			"fw main task",                      /* task name for kernel awareness debugging */
			5000L / sizeof(portSTACK_TYPE),      /* task stack size */
			NULL,                      			 /* optional task startup argument */
			6U,                                  /* initial priority */
			mainTaskHandle					 /* optional task handle to create */
	);

	vTaskStartScheduler();
}

void powerOffFinalStage(void)
{
	uint32_t m;

	// If user was in a private call when they turned the radio off we need to restore the last Tg prior to stating the Private call.
	// to the nonVolatile Setting overrideTG, otherwise when the radio is turned on again it be in PC mode to that station.
	if ((trxTalkGroupOrPcId >> 24) == PC_CALL_FLAG)
	{
		settingsSet(nonVolatileSettings.overrideTG, menuUtilityTgBeforePcMode);
	}

	menuHotspotRestoreSettings();

	m = fw_millis();
	settingsSaveSettings(true);

	// Give it a bit of time before pulling the plug as DM-1801 EEPROM looks slower
	// than GD-77 to write, then quickly power cycling triggers settings reset.
	while (1U)
	{
		if ((fw_millis() - m) > 50)
		{
			break;
		}
	}

	settingsSet(nonVolatileSettings.displayBacklightPercentageOff, 0);
	displayEnableBacklight(false);

#if !defined(PLATFORM_RD5R)
	// This turns the power off to the CPU.
	GPIO_PinWrite(GPIO_Keep_Power_On, Pin_Keep_Power_On, 0);

	// Death trap
	while(1U) {}
#endif
}

static void showLowBattery(void)
{
	ucClearBuf();
	ucPrintCentered(32, currentLanguage->low_battery, FONT_SIZE_3);
	ucRender();
}

static void showErrorMessage(char *message)
{
	ucClearBuf();
	ucPrintCentered(32, message, FONT_SIZE_3);
	ucRender();
}


static void keyBeepHandler(uiEvent_t *ev, bool PTTToggledDown)
{
	// Do not send any beep while scanning, otherwise enabling the AMP will be handled as a valid signal detection.
	if (((ev->keys.event & (KEY_MOD_LONG | KEY_MOD_PRESS)) == (KEY_MOD_LONG | KEY_MOD_PRESS)) ||
			((ev->keys.event & KEY_MOD_UP) == KEY_MOD_UP))
	{
		if ((PTTToggledDown == false) && (uiVFOModeIsScanning() == false) && (uiChannelModeIsScanning() == false))
		{
			if ((nonVolatileSettings.audioPromptMode == AUDIO_PROMPT_MODE_BEEP) || (nonVolatileSettings.audioPromptMode >= AUDIO_PROMPT_MODE_VOICE_LEVEL_1))
			{
				soundSetMelody(nextKeyBeepMelody);
			}
			else
			{
				soundSetMelody(MELODY_KEY_BEEP);
			}
			nextKeyBeepMelody = (int *)MELODY_KEY_BEEP;// set back to the default beep
		}
		else
		{ 	// Reset the beep sound if we are scanning, otherwise the AudioAssist
			// beep will be played instead of the normal one.
			soundSetMelody(MELODY_KEY_BEEP);
		}
	}
	else
	{
		if ((ev->keys.event & (KEY_MOD_LONG | KEY_MOD_DOWN)) == (KEY_MOD_LONG | KEY_MOD_DOWN))
		{
			if ((PTTToggledDown == false) && (uiVFOModeIsScanning() == false) && (uiChannelModeIsScanning() == false))
			{
				soundSetMelody(MELODY_KEY_LONG_BEEP);
			}
		}
	}
}

void mainTask(void *data)
{
	keyboardCode_t keys;
	int key_event;
	int keyFunction;
	uint32_t buttons;
	int button_event;
	uint32_t rotary;
	int rotary_event;
	int function_event;
	uiEvent_t ev = { .buttons = 0, .keys = NO_KEYCODE, .rotary = 0, .function = 0, .events = NO_EVENT, .hasEvent = false, .time = 0 };
	bool keyOrButtonChanged = false;
#if !defined(PLATFORM_GD77S)
	bool wasRestoringDefaultsettings = false;
#endif
	USB_DeviceApplicationInit();

	// Init I2C
	I2C0aInit();
	gpioInitCommon();
	buttonsInit();
	LEDsInit();
	keyboardInit();
	rotarySwitchInit();

	buttonsCheckButtonsEvent(&buttons, &button_event, false);// Read button state and event

	if (buttons & BUTTON_SK2)
	{
#if !defined(PLATFORM_GD77S)
		wasRestoringDefaultsettings = true;
#endif
		settingsRestoreDefaultSettings();
		settingsLoadSettings();
	}
	else
	{
#if !defined(PLATFORM_GD77S)
		wasRestoringDefaultsettings = settingsLoadSettings();
#endif
	}

	displayInit(nonVolatileSettings.displayInverseVideo);

	// Init SPI
	SPIInit();

	// Init I2S
	init_I2S();
	setup_I2S();

	// Init ADC
	adcInit();

	// Init DAC
	dac_init();

	// We shouldn't go further if calibration related initialization has failed
	if ((SPI_Flash_init() == false) || (calibrationInit() == false) || (calibrationCheckAndCopyToCommonLocation(false) == false))
	{
		showErrorMessage("CAL DATA ERROR");
		while(1U)
		{
			tick_com_request();
		}
	}

	// Init AT1846S
	AT1846Init();

	// Init HR-C6000
	SPI_HR_C6000_init();

	// Additional init stuff
	SPI_C6000_postinit();
	AT1846Postinit();

	// Init HR-C6000 interrupts
	init_HR_C6000_interrupts();

	// VOX init
	voxInit();

	// Small startup delay after initialization to stabilize system
	//  vTaskDelay(portTICK_PERIOD_MS * 500);

	init_pit();

	trx_measure_count = 0;

	if (adcGetBatteryVoltage() < CUTOFF_VOLTAGE_UPPER_HYST)
	{
		showLowBattery();
#if !defined(PLATFORM_RD5R)
		GPIO_PinWrite(GPIO_Keep_Power_On, Pin_Keep_Power_On, 0);
#endif
		while(1U) {};
	}

	init_hrc6000_task();

	menuBatteryInit(); // Initialize circular buffer
	init_watchdog(menuBatteryPushBackVoltage);

	soundInitBeepTask();

#if defined(USE_SEGGER_RTT)
	SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
	SEGGER_RTT_printf(0,"Segger RTT initialised\n");
#endif

	// Clear boot melody and image
#if defined(PLATFORM_GD77S)
	if ((buttons & (BUTTON_SK2 | BUTTON_ORANGE)) == ((BUTTON_SK2 | BUTTON_ORANGE)))
#else
	if ((buttons & BUTTON_SK2) && ((keyboardRead() & (SCAN_UP | SCAN_DOWN)) == (SCAN_UP | SCAN_DOWN)))
#endif
	{
		settingsEraseCustomContent();
	}

	lastheardInitList();
	codeplugInitContactsCache();
	dmrIDCacheInit();
	voicePromptsCacheInit();
#if !defined(PLATFORM_GD77S)
	// Note GD77S has always has voice prompt level 3 enabled by default so no need to do this check on that radio
	if (wasRestoringDefaultsettings)
	{
		enableVoicePromptsIfLoaded();
	}
#endif

	// Should be initialized before the splash screen, as we don't want melodies when VOX is enabled
	voxSetParameters(nonVolatileSettings.voxThreshold, nonVolatileSettings.voxTailUnits);

#if defined(PLATFORM_GD77S)
	// Those act as a toggles

	// Band limits
	if ((buttons & (BUTTON_SK1 | BUTTON_PTT)) == (BUTTON_SK1 | BUTTON_PTT))
	{
		settingsSet(nonVolatileSettings.txFreqLimited, !nonVolatileSettings.txFreqLimited);

		voicePromptsInit();
		voicePromptsAppendLanguageString(&currentLanguage->band_limits);
		voicePromptsAppendLanguageString(nonVolatileSettings.txFreqLimited ? &currentLanguage->on : &currentLanguage->off);
		voicePromptsPlay();
	}
	// Hotspot mode
	else if ((buttons & BUTTON_SK1) == BUTTON_SK1)
	{
		settingsSet(nonVolatileSettings.hotspotType, ((nonVolatileSettings.hotspotType == HOTSPOT_TYPE_MMDVM) ? HOTSPOT_TYPE_BLUEDV : HOTSPOT_TYPE_MMDVM));

		voicePromptsInit();
		voicePromptsAppendLanguageString(&currentLanguage->hotspot_mode);
		voicePromptsAppendString((nonVolatileSettings.hotspotType == HOTSPOT_TYPE_MMDVM) ? "MMDVM" : "BlueDV");
		voicePromptsPlay();
	}
#endif

	menuInitMenuSystem();

	// Reset buttons/key states in case some where pressed while booting.
	button_event = EVENT_BUTTON_NONE;
	buttons = BUTTON_NONE;
	key_event = EVENT_KEY_NONE;
	keys.event = 0;
	keys.key = 0;

	lowbatteryTimer = fw_millis() + 5000;// Check battery 5 seconds after the firmware starts

#if !defined(PLATFORM_GD77S)
	wasRestoringDefaultsettings = false;
#endif

	while (1U)
	{
		taskENTER_CRITICAL();
		uint32_t tmp_timer_maintask = timer_maintask;
		taskEXIT_CRITICAL();

		if (tmp_timer_maintask == 0)
		{
			taskENTER_CRITICAL();
			timer_maintask = 10;
			alive_maintask = true;
			taskEXIT_CRITICAL();

			tick_com_request();

			keyboardCheckKeyEvent(&keys, &key_event); // Read keyboard state and event

			buttonsCheckButtonsEvent(&buttons, &button_event, (keys.key != 0)); // Read button state and event

			rotarySwitchCheckRotaryEvent(&rotary, &rotary_event); // Rotary switch state and event (GD-77S only)

			// VOX Checking
			if (voxIsEnabled())
			{
				// if a key/button event happen, reset the VOX.
				if ((key_event == EVENT_KEY_CHANGE) || (button_event == EVENT_BUTTON_CHANGE) || (keys.key != 0) || (buttons != BUTTON_NONE))
				{
					voxReset();
				}
				else
				{
					if (!trxTransmissionEnabled && voxIsTriggered() && ((buttons & BUTTON_PTT) == 0))
					{
						button_event = EVENT_BUTTON_CHANGE;
						buttons |= BUTTON_PTT;
					}
					else if (trxTransmissionEnabled && ((voxIsTriggered() == false) || (keys.event & KEY_MOD_PRESS)))
					{
						button_event = EVENT_BUTTON_CHANGE;
						buttons &= ~BUTTON_PTT;
					}
					else if (trxTransmissionEnabled && voxIsTriggered())
					{
						// Any key/button event reset the vox
						if ((button_event != EVENT_BUTTON_NONE) || (keys.event != EVENT_KEY_NONE))
						{
							voxReset();
							button_event = EVENT_BUTTON_CHANGE;
							buttons &= ~BUTTON_PTT;
						}
						else
						{
							buttons |= BUTTON_PTT;
						}
					}
				}
			}


			// EVENT_*_CHANGED can be cleared later, so check this now as hasEvent has to be set anyway.
			keyOrButtonChanged = ((key_event != NO_EVENT) || (button_event != NO_EVENT) || (rotary_event != NO_EVENT));

			if (batteryVoltageHasChanged == true)
			{
				int currentMenu = menuSystemGetCurrentMenuNumber();

				if ((currentMenu == UI_CHANNEL_MODE) || (currentMenu == UI_VFO_MODE))
				{
#if defined(PLATFORM_RD5R)
					ucFillRect(0, 0, 128, 8, true);
#else
					ucClearRows(0, 2, false);
#endif
					menuUtilityRenderHeader();
					ucRenderRows(0, 2);
				}

				batteryVoltageHasChanged = false;
			}

			if (keypadLocked || PTTLocked)
			{
				if (keypadLocked && ((buttons & BUTTON_PTT) == 0))
				{
					if (key_event == EVENT_KEY_CHANGE)
					{
						if ((PTTToggledDown == false) && (menuSystemGetCurrentMenuNumber() != UI_LOCK_SCREEN))
						{
							menuSystemPushNewMenu(UI_LOCK_SCREEN);
						}

						key_event = EVENT_KEY_NONE;

						if (nonVolatileSettings.pttToggle && PTTToggledDown)
						{
							PTTToggledDown = false;
						}
					}

					// Lockout ORANGE AND BLUE (BLACK stay active regardless lock status, useful to trigger backlight)
#if defined(PLATFORM_RD5R)
					if ((button_event == EVENT_BUTTON_CHANGE) && (buttons & BUTTON_SK2))
#else
					if ((button_event == EVENT_BUTTON_CHANGE) && ((buttons & BUTTON_ORANGE) || (buttons & BUTTON_SK2)))
#endif
					{
						if ((PTTToggledDown == false) && (menuSystemGetCurrentMenuNumber() != UI_LOCK_SCREEN))
						{
							menuSystemPushNewMenu(UI_LOCK_SCREEN);
						}

						button_event = EVENT_BUTTON_NONE;

						if (nonVolatileSettings.pttToggle && PTTToggledDown)
						{
							PTTToggledDown = false;
						}
					}
				}
				else if (PTTLocked)
				{
					if ((buttons & BUTTON_PTT) && (button_event == EVENT_BUTTON_CHANGE))
					{
						soundSetMelody(MELODY_ERROR_BEEP);

						if (menuSystemGetCurrentMenuNumber() != UI_LOCK_SCREEN)
						{
							menuSystemPushNewMenu(UI_LOCK_SCREEN);
						}

						button_event = EVENT_BUTTON_NONE;
						// Clear PTT button
						buttons &= ~BUTTON_PTT;
					}
					else if ((buttons & BUTTON_SK2) && KEYCHECK_DOWN(keys, KEY_STAR))
					{
						if (menuSystemGetCurrentMenuNumber() != UI_LOCK_SCREEN)
						{
							menuSystemPushNewMenu(UI_LOCK_SCREEN);
						}
					}
				}
			}

			int trxMode = trxGetMode();

#if ! defined(PLATFORM_GD77S)
			if ((key_event == EVENT_KEY_CHANGE) && ((buttons & BUTTON_PTT) == 0) && (keys.key != 0))
			{
				if (KEYCHECK_LONGDOWN(keys, KEY_RED) && (uiVFOModeIsScanning() == false) && (uiChannelModeIsScanning() == false))
				{
					contactListContactIndex = 0;
					menuSystemPopAllAndDisplayRootMenu();
				}
			}

			/*
			 * This code ignores the keypress if the display is not lit, however this functionality is proving to be a problem for things like entering a TG
			 * I think it needs to be removed until a better solution can be found
			 *
			if (menuDisplayLightTimer == 0
					&& nonVolatileSettings.backLightTimeout != 0)
			{
				if (key_event == EVENT_KEY_CHANGE)
				{
					key_event = EVENT_KEY_NONE;
					keys = 0;
					displayLightTrigger();
				}
				if (button_event == EVENT_BUTTON_CHANGE && (buttons & BUTTON_ORANGE) != 0)
				{
					button_event = EVENT_BUTTON_NONE;
					buttons = 0;
					displayLightTrigger();
				}
			}
			 */


			//
			// PTT toggle feature
			//
			// PTT is locked down, but any button, except SK1 or SK2(1750Hz in FM) or DTMF Key in Analog, is pressed, virtually release PTT
			if ((nonVolatileSettings.pttToggle && PTTToggledDown) &&
					(((button_event & EVENT_BUTTON_CHANGE) && (
#if ! defined(PLATFORM_RD5R)
							(buttons & BUTTON_ORANGE) ||
#endif
							((trxMode != RADIO_MODE_ANALOG) && (buttons & BUTTON_SK2)))) ||
							((keys.key != 0) && (keys.event & KEY_MOD_UP) && (((trxMode == RADIO_MODE_ANALOG) && keyboardKeyIsDTMFKey(keys.key)) == false))))
			{
				PTTToggledDown = false;
				button_event = EVENT_BUTTON_CHANGE;
				buttons = BUTTON_NONE;
				key_event = NO_EVENT;
				keys.key = 0;
			}

			// PTT toggle action
			if (nonVolatileSettings.pttToggle)
			{
				if (button_event == EVENT_BUTTON_CHANGE)
				{
					if (buttons & BUTTON_PTT)
					{
						if (PTTToggledDown == false)
						{
							// PTT toggle works only if a TOT value is defined.
							if (currentChannelData->tot != 0)
							{
								PTTToggledDown = true;
							}
						}
						else
						{
							PTTToggledDown = false;
						}
					}
				}

				if (PTTToggledDown && ((buttons & BUTTON_PTT) == 0))
				{
					buttons |= BUTTON_PTT;
				}
			}
			else
			{
				if (PTTToggledDown)
				{
					PTTToggledDown = false;
				}
			}
#endif

			if (button_event == EVENT_BUTTON_CHANGE)
			{
				displayLightTrigger();
				if ((buttons & BUTTON_PTT) != 0)
				{
					int currentMenu = menuSystemGetCurrentMenuNumber();

					/*
					 * This code would prevent transmission on simplex if the radio is receiving a DMR signal.
					 * if ((slot_state == DMR_STATE_IDLE || trxDMRMode == DMR_MODE_PASSIVE)  &&
					 *
					 */
					if ((trxMode != RADIO_MODE_NONE) &&
							(settingsUsbMode != USB_MODE_HOTSPOT) &&
							(currentMenu != UI_POWER_OFF) &&
							(currentMenu != UI_SPLASH_SCREEN) &&
							(currentMenu != UI_TX_SCREEN))
					{
						bool wasScanning = false;

						if (scanToneActive)
						{
							uiVFOModeStopScanning();
						}

						if (scanActive)
						{
							if (currentMenu == UI_VFO_MODE)
							{
								uiVFOModeStopScanning();
							}
							else
							{
								uiChannelModeStopScanning();
							}
							wasScanning = true;
						}
						else
						{
							if (currentMenu == UI_LOCK_SCREEN)
							{
								menuLockScreenPop();
							}
						}
						if (!wasScanning)
						{
							if ((menuSystemGetCurrentMenuNumber() == UI_PRIVATE_CALL) && (nonVolatileSettings.privateCalls == ALLOW_PRIVATE_CALLS_PTT))
							{
								acceptPrivateCall(menuUtilityReceivedPcId);
								menuSystemPopPreviousMenu();
							}
							else if ((menuSystemGetCurrentMenuNumber() == MENU_CONTACT_LIST_SUBMENU) && menuContactListIsDTMFSequenceKeying())
							{
								menuContactListDTMFSequenceReset();
							}

							menuSystemPushNewMenu(UI_TX_SCREEN);
						}
					}
				}

				if ((buttons & BUTTON_SK1) && (buttons & BUTTON_SK2))
				{
					settingsSaveSettings(true);
				}

				// Toggle backlight
				if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_MANUAL) && (buttons & BUTTON_SK1))
				{
					displayEnableBacklight(! displayIsBacklightLit());
				}
			}

			if (!trxTransmissionEnabled && (updateLastHeard == true))
			{
				lastHeardListUpdate((uint8_t *)DMR_frame_buffer, false);
				updateLastHeard = false;
			}

			if ((nonVolatileSettings.hotspotType == HOTSPOT_TYPE_OFF) ||
					((nonVolatileSettings.hotspotType != HOTSPOT_TYPE_OFF) && (settingsUsbMode != USB_MODE_HOTSPOT))) // Do not filter anything in HS mode.
			{
				if ((uiPrivateCallState == PRIVATE_CALL_DECLINED) &&
						(slot_state == DMR_STATE_IDLE))
				{
					menuClearPrivateCall();
				}
				if (!trxTransmissionEnabled && (menuDisplayQSODataState == QSO_DISPLAY_CALLER_DATA) && (nonVolatileSettings.privateCalls > ALLOW_PRIVATE_CALLS_OFF))
				{
					if (HRC6000GetReceivedTgOrPcId() == (trxDMRID | (PC_CALL_FLAG << 24)))
					{
						if ((uiPrivateCallState == NOT_IN_CALL) &&
								(trxTalkGroupOrPcId != (HRC6000GetReceivedSrcId() | (PC_CALL_FLAG << 24))) &&
								(HRC6000GetReceivedSrcId() != uiPrivateCallLastID))
						{
							if ((HRC6000GetReceivedSrcId() & 0xFFFFFF) >= 1000000)
							{
								/*
								if (nonVolatileSettings.privateCalls == ALLOW_PRIVATE_CALLS_AUTO)
								{
									acceptPrivateCall(menuUtilityReceivedPcId);
								}
								else
								{
								*/
									menuSystemPushNewMenu(UI_PRIVATE_CALL);
								/*
								}*/
							}
						}
					}
				}
			}

#if defined(PLATFORM_GD77S) && defined(READ_CPUID)
			if ((buttons & (BUTTON_SK1 | BUTTON_ORANGE | BUTTON_PTT)) == (BUTTON_SK1 | BUTTON_ORANGE | BUTTON_PTT))
			{
				debugReadCPUID();
			}
#endif

			ev.function = 0;
			function_event = NO_EVENT;
			if (buttons & BUTTON_SK2)
			{
				//				keyFunction = codeplugGetQuickkeyFunctionID(keys.key);
				switch (keys.key)
				{
				case '0':
					keyFunction = ( MENU_SOUND << 8) | 2;
					break;
				case '1':
					keyFunction = START_SCANNING;
					break;
				case '2':
					keyFunction = (MENU_BATTERY << 8);
					break;
				case '3':
					keyFunction = ( MENU_LAST_HEARD << 8);
					break;
				case '4':
					keyFunction = ( MENU_CHANNEL_DETAILS << 8);
					break;
				case '5':
					keyFunction = ( MENU_OPTIONS << 8) | 2;
					break;
				case '6':
					keyFunction = ( MENU_DISPLAY << 8) | 2;
					break;
				case '7':
					keyFunction = (MENU_DISPLAY <<8) + DEC_BRIGHTNESS;
					break;
				case '8':
					keyFunction = (MENU_DISPLAY <<8) + INC_BRIGHTNESS;
					break;
				case '9':
					keyFunction = ( MENU_RSSI_SCREEN << 8);
					break;
				default:
					keyFunction = 0;
					break;
				}

				if (keyFunction > 0)
				{
					int menuFunction = (keyFunction >> 8) & 0xff;

					if (menuFunction > 0 && menuFunction < NUM_MENU_ENTRIES)
					{
						int currentMenu = menuSystemGetCurrentMenuNumber();
						if (currentMenu != menuFunction)
						{
							menuSystemPushNewMenu(menuFunction);
						}
					}
					ev.function = keyFunction & 0xff;
					buttons = BUTTON_NONE;
					rotary = 0;
					key_event = EVENT_KEY_NONE;
					button_event = EVENT_BUTTON_NONE;
					rotary_event = EVENT_ROTARY_NONE;
					keys.key = 0;
					keys.event = 0;
					function_event = FUNCTION_EVENT;
					keyboardReset();
				}
			}
			ev.buttons = buttons;
			ev.keys = keys;
			ev.rotary = rotary;
			ev.events = function_event | (button_event << 1) | (rotary_event << 3) | key_event;
			ev.hasEvent = keyOrButtonChanged || function_event;
			ev.time = fw_millis();

			/*
			 * We probably can't terminate voice prompt playback in main, because some screens need to a follow-on playback if the prompt was playing when a button was pressed
			 *
			if ((nonVolatileSettings.audioPromptMode == AUDIO_PROMPT_MODE_SILENT || voicePromptIsActive)   && (ev.keys.event & KEY_MOD_DOWN))
			{
				voicePromptsTerminate();
			}
			*/

			menuSystemCallCurrentMenuTick(&ev);

			// Beep sounds aren't allowed in these modes.
			if (((nonVolatileSettings.audioPromptMode == AUDIO_PROMPT_MODE_SILENT) || voicePromptsIsPlaying()) /*|| (nonVolatileSettings.audioPromptMode == AUDIO_PROMPT_MODE_VOICE)*/)
			{
				if (melody_play != NULL)
				{
					melody_play = NULL;
				}

				// AMBE thing, if any
				if ((buttons & BUTTON_PTT) == 0)
				{

				}
			}
			else
			{
				if ((((key_event == EVENT_KEY_CHANGE) || (button_event == EVENT_BUTTON_CHANGE))
						&& ((buttons & BUTTON_PTT) == 0) && (ev.keys.key != 0))
						|| (function_event == FUNCTION_EVENT))
				{
					if (function_event == FUNCTION_EVENT)
					{
						ev.keys.event |= KEY_MOD_UP;
					}

					keyBeepHandler(&ev, PTTToggledDown);
				}
			}

#if defined(PLATFORM_RD5R)
			if (keyFunction == TOGGLE_TORCH)
			{
				toggle_torch();
			}
#endif


#if defined(PLATFORM_GD77S)
			if (trxTransmissionEnabled == false &&
#else
				if ((menuSystemGetCurrentMenuNumber() != UI_TX_SCREEN) &&
#endif
				(battery_voltage < (CUTOFF_VOLTAGE_LOWER_HYST + LOW_BATTERY_WARNING_VOLTAGE_DIFFERENTIAL))	&&
				(fw_millis() > lowbatteryTimer))
			{

				if (melody_play == NULL)
				{
					if (nonVolatileSettings.audioPromptMode < AUDIO_PROMPT_MODE_VOICE_LEVEL_1)
					{
						soundSetMelody(MELODY_LOW_BATTERY);
					}
					else
					{
						voicePromptsInit();
						voicePromptsAppendLanguageString(&currentLanguage->low_battery);
						voicePromptsPlay();
					}
					lowbatteryTimer = fw_millis() + LOW_BATTERY_INTERVAL;
				}
			}

#if defined(PLATFORM_RD5R)
			if ((battery_voltage < CUTOFF_VOLTAGE_LOWER_HYST)
					&& (menuSystemGetCurrentMenuNumber() != UI_POWER_OFF))
#else
			if (((GPIO_PinRead(GPIO_Power_Switch, Pin_Power_Switch) != 0)
					|| (battery_voltage < CUTOFF_VOLTAGE_LOWER_HYST))
					&& (menuSystemGetCurrentMenuNumber() != UI_POWER_OFF))
#endif
			{
				if (battery_voltage < CUTOFF_VOLTAGE_LOWER_HYST)
				{
					showLowBattery();
#if defined(PLATFORM_RD5R)
					powerOffFinalStage();
#else
					if (GPIO_PinRead(GPIO_Power_Switch, Pin_Power_Switch) != 0)
					{
						powerOffFinalStage();
					}
				}
				else
				{
					menuSystemPushNewMenu(UI_POWER_OFF);
#endif // ! PLATFORM_RD5R
				}
				GPIO_PinWrite(GPIO_audio_amp_enable, Pin_audio_amp_enable, 0);
				soundSetMelody(NULL);
			}

			if (((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_AUTO)
					|| (nonVolatileSettings.backlightMode == BACKLIGHT_MODE_SQUELCH)) && (menuDisplayLightTimer > 0))
			{
				// Countdown only in (AUTO) or (SQUELCH + no audio)
				if ((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_AUTO) ||
						((nonVolatileSettings.backlightMode == BACKLIGHT_MODE_SQUELCH) && ((getAudioAmpStatus() & AUDIO_AMP_MODE_RF) == 0)))
				{
					menuDisplayLightTimer--;
				}

				if (menuDisplayLightTimer == 0)
				{
					displayEnableBacklight(false);
				}
			}

			if (voicePromptsIsPlaying())
			{
				voicePromptsTick();
			}
			soundTickMelody();
			voxTick();

#if defined(PLATFORM_RD5R) // Needed for platforms which can't control the poweroff
			settingsSaveIfNeeded(false);
#endif
		}
		vTaskDelay(0);
	}
}

#if defined(READ_CPUID)
void debugReadCPUID(void)
{
	char tmp[6];
	char buf[512]={0};
	uint8_t *p = (uint8_t *)0x40048054;
	USB_DEBUG_PRINT("\nCPU ID\n");
	vTaskDelay(portTICK_PERIOD_MS * 1);
	for(int i = 0; i < 16; i++)
	{
		sprintf(tmp, "%02x ", *p);
		strcat(buf, tmp);
		p++;
	}
	USB_DEBUG_PRINT(buf);

	vTaskDelay(portTICK_PERIOD_MS * 1);
	USB_DEBUG_PRINT("\nProtection bytes\n");
	vTaskDelay(portTICK_PERIOD_MS * 1);

	buf[0] = 0;
#if defined(PLATFORM_DM1801)
	p = (uint8_t *)0x3800;
#else
	p = (uint8_t *)0x7f800;
#endif
	for(int i = 0; i < 36; i++)
	{
		sprintf(tmp, "%02x ", *p);
		strcat(buf, tmp);
		p++;
	}
	USB_DEBUG_PRINT(buf);
}
#endif
