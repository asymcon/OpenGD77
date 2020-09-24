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
#include <HR-C6000.h>
#include <settings.h>
#include <sound.h>
#include <user_interface/menuSystem.h>
#include <user_interface/uiUtilities.h>
#include <user_interface/uiLocalisation.h>


static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);

static const int PIT_COUNTS_PER_SECOND = 10000;
static int timeInSeconds;
static uint32_t nextSecondPIT;
static bool isShowingLastHeard;
extern bool PTTToggledDown;
static bool startBeepPlayed;

menuStatus_t menuTxScreen(uiEvent_t *ev, bool isFirstRun)
{
	static uint32_t m = 0, micm = 0, mto = 0;

	if (isFirstRun)
	{
		voicePromptsTerminate();
		startBeepPlayed = false;
		scanActive = false;
		trxIsTransmittingTone = false;
		isShowingLastHeard = false;

		if (((currentChannelData->flag4 & 0x04) == 0x00) && (trxCheckFrequencyInAmateurBand(currentChannelData->txFreq) || (nonVolatileSettings.txFreqLimited == false)))
		{
			nextSecondPIT = PITCounter + PIT_COUNTS_PER_SECOND;
			timeInSeconds = currentChannelData->tot * 15;

			GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
			GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 1);

			txstopdelay = 0;
			clearIsWakingState();
			if (trxGetMode() == RADIO_MODE_ANALOG)
			{
				trxSetTxCSS(currentChannelData->txTone);
			}
			trx_setTX();
			updateScreen();
		}
		else
		{
			// Note.
			// Currently these messages are not being displayed, because this screen gets immediately after the display is update
			// However the melody will play
			//
			// We need to work out how to display this message for 1 or 2 seconds, even if the PTT is released.
			// But this would require some sort of timer callback system, which we don't currently have.
			//
			ucClearBuf();

			ucDrawRoundRectWithDropShadow(4, 4, 120, (DISPLAY_SIZE_Y - 6), 5, true);
			ucPrintCentered(4, currentLanguage->error, FONT_SIZE_4);

			if ((currentChannelData->flag4 & 0x04) != 0x00)
			{
				ucPrintCentered((DISPLAY_SIZE_Y - 24), currentLanguage->rx_only, FONT_SIZE_3);
			}
			else
			{
				ucPrintCentered((DISPLAY_SIZE_Y - 24), currentLanguage->out_of_band, FONT_SIZE_3);
			}
			ucRender();
			displayLightOverrideTimeout(-1);
			soundSetMelody(MELODY_ERROR_BEEP);
			PTTToggledDown = false;
		}

		m = micm = ev->time;
	}
	else
	{

#if defined(PLATFORM_GD77S)
		heartBeatActivityForGD77S(ev);
#endif

		if (trxTransmissionEnabled && (getIsWakingState() == WAKING_MODE_NONE))
		{
			if (PITCounter >= nextSecondPIT)
			{
				if (currentChannelData->tot == 0)
				{
					timeInSeconds++;
				}
				else
				{
					timeInSeconds--;
					if (timeInSeconds <= (nonVolatileSettings.txTimeoutBeepX5Secs * 5))
					{
						if ((timeInSeconds % 5) == 0)
						{
							soundSetMelody(MELODY_KEY_BEEP);
						}
					}
				}

				if ((currentChannelData->tot != 0) && (timeInSeconds == 0))
				{
					soundSetMelody(MELODY_TX_TIMEOUT_BEEP);

					ucClearBuf();
					ucPrintCentered(20, currentLanguage->timeout, FONT_SIZE_4);
					ucRender();
					PTTToggledDown = false;
					mto = ev->time;
					voxReset();
				}
				else
				{
					if (!isShowingLastHeard)
					{
						updateScreen();
					}
				}

				nextSecondPIT = PITCounter + PIT_COUNTS_PER_SECOND;
			}
			else
			{
				int mode = trxGetMode();

				if (mode == RADIO_MODE_DIGITAL)
				{
					if ((nonVolatileSettings.beepOptions & BEEP_TX_START) &&
							(startBeepPlayed == false) && (trxIsTransmitting == true)
							&& (melody_play == NULL))
					{
						startBeepPlayed = true;// set this even if the beep is not actaully played because of the vox, as otherwise this code will get continuously run
						// If VOX is running, don't send a beep as it will reset its the trigger status.
						if ((voxIsEnabled() == false) || (voxIsEnabled() && (voxIsTriggered() == false)))
						{
							soundSetMelody(MELODY_DMR_TX_START_BEEP);
						}
					}
				}

				if ((ev->time - micm) > 100)
				{
					if (mode == RADIO_MODE_DIGITAL)
					{
						drawDMRMicLevelBarGraph();
					}
					else
					{
						drawFMMicLevelBarGraph();
					}

					ucRenderRows(1, 2);
					micm = ev->time;
				}

			}
		}

		// Timeout happened, postpone going further otherwise timeout
		// screen won't be visible at all.
		if ((currentChannelData->tot != 0) && (timeInSeconds == 0))
		{
			if ((ev->time - mto) < 500)
			{
				return MENU_STATUS_SUCCESS;
			}
		}


		// Got an event, or
		if (ev->hasEvent || // PTT released, Timeout triggered,
				( (((ev->buttons & BUTTON_PTT) == 0) || ((currentChannelData->tot != 0) && (timeInSeconds == 0))) ||
						// or waiting for DMR ending (meanwhile, updating every 100ms)
						((trxTransmissionEnabled == false) && ((ev->time - m) > 100))))
		{
			handleEvent(ev);
			m = ev->time;
		}
	}
	return MENU_STATUS_SUCCESS;
}

static void updateScreen(void)
{
	menuDisplayQSODataState = QSO_DISPLAY_DEFAULT_SCREEN;
	if (menuControlData.stack[0] == UI_VFO_MODE)
	{
		uiVFOModeUpdateScreen(timeInSeconds);
		displayLightOverrideTimeout(-1);
	}
	else
	{
		uiChannelModeUpdateScreen(timeInSeconds);
		displayLightOverrideTimeout(-1);
	}
}

static void handleEvent(uiEvent_t *ev)
{
	// Xmiting ends (normal or timeouted)
	if (((ev->buttons & BUTTON_PTT) == 0)
			|| ((currentChannelData->tot != 0) && (timeInSeconds == 0)))
	{
		if (trxTransmissionEnabled)
		{
			trxTransmissionEnabled = false;
			trxIsTransmittingTone = false;

			if (trxGetMode() == RADIO_MODE_ANALOG)
			{
				// In analog mode. Stop transmitting immediately
				GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 0);

				// Need to wrap this in Task Critical to avoid bus contention on the I2C bus.
				taskENTER_CRITICAL();
				trxSetRxCSS(currentChannelData->rxTone);
				trxActivateRx();
				trxIsTransmitting = false;
				taskEXIT_CRITICAL();
				menuSystemPopPreviousMenu();
			}
			else
			{
				if (isShowingLastHeard)
				{
					isShowingLastHeard = false;
					updateScreen();
				}
			}
			// When not in analogue mode, only the trxIsTransmitting flag is cleared
			// This screen keeps getting called via the handleEvent function and goes into the else clause - below.
		}
		else
		{
			// In DMR mode, wait for the DMR system to finish before exiting

			if (!trxIsTransmitting)
			{
				if ((nonVolatileSettings.beepOptions & BEEP_TX_STOP) && (melody_play == NULL))
				{
					soundSetMelody(MELODY_DMR_TX_STOP_BEEP);
				}

				GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 0);

				// If there is a signal, lit the Green LED
				if ((GPIO_PinRead(GPIO_LEDgreen, Pin_LEDgreen) == 0) && (trxCarrierDetected() || (getAudioAmpStatus() & AUDIO_AMP_MODE_RF)))
				{
					GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 1);
				}

				menuSystemPopPreviousMenu();
			}
		}
	}

	// Key action while xmitting (ANALOG), Tone triggering
	if (!trxIsTransmittingTone && ((ev->buttons & BUTTON_PTT) != 0) && trxTransmissionEnabled && (trxGetMode() == RADIO_MODE_ANALOG))
	{
		// Send 1750Hz
		if (BUTTONCHECK_DOWN(ev, BUTTON_SK2))
		{
			trxIsTransmittingTone = true;
			trxSetTone1(1750);
			trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_TONE1);
			enableAudioAmp(AUDIO_AMP_MODE_RF);
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1);
		}
		else
		{ // Send DTMF
			int keyval = menuGetKeypadKeyValue(ev, false);

			if (keyval != 99)
			{
				trxSetDTMF(keyval);
				trxIsTransmittingTone = true;
				trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_DTMF);
				enableAudioAmp(AUDIO_AMP_MODE_RF);
				GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1);
			}
		}
	}

	// Stop xmitting Tone
	if (trxIsTransmittingTone && (BUTTONCHECK_DOWN(ev, BUTTON_SK2) == 0) && ((ev->keys.key == 0) || (ev->keys.event & KEY_MOD_UP)))
	{
		trxIsTransmittingTone = false;
		trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_MIC);
		disableAudioAmp(AUDIO_AMP_MODE_RF);
	}

	if ((trxGetMode() == RADIO_MODE_DIGITAL) && BUTTONCHECK_DOWN(ev, BUTTON_SK1) && (isShowingLastHeard == false) && (trxTransmissionEnabled == true))
	{
		isShowingLastHeard = true;
		menuLastHeardUpdateScreen(false, false, false);
	}
	else
	{
		if (isShowingLastHeard && (BUTTONCHECK_DOWN(ev, BUTTON_SK1) == 0))
		{
			isShowingLastHeard = false;
			updateScreen();
		}
	}

}
