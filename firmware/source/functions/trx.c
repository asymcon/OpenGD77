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

#include <AT1846S.h>
#include <calibration.h>
#include <HR-C6000.h>
#include <settings.h>
#include <trx.h>
#include <user_interface/menuSystem.h>


int trx_measure_count = 0;
volatile bool trxIsTransmitting = false;
uint32_t trxTalkGroupOrPcId = 9;// Set to local TG just in case there is some problem with it not being loaded
uint32_t trxDMRID = 0;// Set ID to 0. Not sure if its valid. This value needs to be loaded from the codeplug.
int txstopdelay = 0;
volatile bool trxIsTransmittingTone = false;
static uint16_t txPower;
static bool analogSignalReceived = false;
static bool digitalSignalReceived = false;

int trxCurrentBand[2] = {RADIO_BAND_VHF,RADIO_BAND_VHF};// Rx and Tx band.

#if USE_DATASHEET_RANGES
const frequencyBand_t RADIO_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM] =  {
													{
														.minFreq=13400000,
														.maxFreq=17400000
													},// VHF
													{
														.minFreq=20000000,
														.maxFreq=26000000
													},// 220Mhz
													{
														.minFreq=40000000,
														.maxFreq=52000000
													}// UHF
};
#else
const frequencyBand_t RADIO_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM] =  {
													{
														.calTableMinFreq = 13400000,
														.minFreq=12700000,
														.maxFreq=17800000
													},// VHF
													{
														.calTableMinFreq = 13400000,
														.minFreq=19000000,
														.maxFreq=28200000
													},// 220Mhz
													{
														.calTableMinFreq = 40000000,
														.minFreq=38000000,
														.maxFreq=56400000
													}// UHF
};
#endif

static const int TRX_SQUELCH_MAX = 70;
const int TRX_CTCSS_TONE_NONE = 65535;
const int TRX_NUM_CTCSS=52;
const unsigned int TRX_CTCSSTones[]={65535,625,670,693,719,744,770,797,825,854,
										885,915,948,974,1000,1035,1072,1109,1148,
										1188,1230,1273,1318,1365,1413,1462,1514,
										1567,1598,1622,1655,1679,1713,1738,1773,
										1799,1835,1862,1899,1928,1966,1995,2035,
										2065,2107,2181,2257,2291,2336,2418,2503,2541};
static const int BAND_VHF_MIN 	= 14400000;
static const int BAND_VHF_MAX 	= 14800000;
static const int BAND_222_MIN 	= 22200000;
static const int BAND_222_MAX 	= 22500000;
static const int BAND_UHF_MIN 	= 42000000;
static const int BAND_UHF_MAX 	= 45000000;

enum CAL_DEV_TONE_INDEXES { CAL_DEV_DTMF = 0, CAL_DEV_TONE = 1, CAL_DEV_CTCSS_WIDE	= 2,CAL_DEV_CTCSS_NARROW = 3,CAL_DEV_DCS_WIDE = 4, CAL_DEV_DCS_NARROW	= 5};

static int currentMode = RADIO_MODE_NONE;
static bool currentBandWidthIs25kHz = BANDWIDTH_12P5KHZ;
static int currentRxFrequency = 14400000;
static int currentTxFrequency = 14400000;
static int currentCC = 1;
static uint8_t squelch = 0x00;
static bool rxCTCSSactive = false;

// AT-1846 native values for Rx
static uint8_t rx_fl_l;
static uint8_t rx_fl_h;
static uint8_t rx_fh_l;
static uint8_t rx_fh_h;

// AT-1846 native values for Tx
static uint8_t tx_fl_l;
static uint8_t tx_fl_h;
static uint8_t tx_fh_l;
static uint8_t tx_fh_h;

volatile uint8_t trxRxSignal;
volatile uint8_t trxRxNoise;
volatile uint8_t trxTxVox;
volatile uint8_t trxTxMic;

static uint8_t trxSaveVoiceGainTx = 0xff;
static uint16_t trxSaveDeviation = 0xff;
static uint8_t voice_gain_tx = 0x31; // default voice_gain_tx fro calibration, needs to be declared here in case calibration:OFF

int trxDMRMode = DMR_MODE_ACTIVE;// Active is for simplex
static volatile bool txPAEnabled=false;

static int trxCurrentDMRTimeSlot;

const int trxDTMFfreq1[] = { 1336, 1209, 1336, 1477, 1209, 1336, 1477, 1209, 1336, 1477, 1633, 1633, 1633, 1633, 1209, 1477 };
const int trxDTMFfreq2[] = {  941,  697,  697,  697,  770,  770,  770,  852,  852,  852,  697,  770,  852,  941,  941,  941 };

calibrationPowerValues_t trxPowerSettings;

int	trxGetMode(void)
{
	return currentMode;
}

int	trxGetBandwidthIs25kHz(void)
{
	return currentBandWidthIs25kHz;
}

void trxSetModeAndBandwidth(int mode, bool bandwidthIs25kHz)
{
	if ((mode != currentMode) || (bandwidthIs25kHz != currentBandWidthIs25kHz))
	{
		currentMode=mode;

		currentBandWidthIs25kHz=bandwidthIs25kHz;

		taskENTER_CRITICAL();
		switch(mode)
		{
		case RADIO_MODE_NONE:
			// not truely off
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 0); // connect AT1846S audio to HR_C6000
			terminate_sound();
			terminate_digital();

			I2C_AT1846_SetMode();
			I2C_AT1846_SetBandwidth();
			trxUpdateC6000Calibration();
			trxUpdateAT1846SCalibration();
			break;
		case RADIO_MODE_ANALOG:
			GPIO_PinWrite(GPIO_TX_audio_mux, Pin_TX_audio_mux, 0); // Connect mic to mic input of AT-1846
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1); // connect AT1846S audio to speaker
			terminate_sound();
			terminate_digital();

			I2C_AT1846_SetMode();
			I2C_AT1846_SetBandwidth();
			trxUpdateC6000Calibration();
			trxUpdateAT1846SCalibration();
			break;
		case RADIO_MODE_DIGITAL:
			I2C_AT1846_SetMode();// Also sets the bandwidth to 12.5kHz which is the standard for DMR

			trxUpdateC6000Calibration();
			trxUpdateAT1846SCalibration();
			GPIO_PinWrite(GPIO_TX_audio_mux, Pin_TX_audio_mux, 1); // Connect mic to MIC_P input of HR-C6000
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 0); // connect AT1846S audio to HR_C6000
			init_sound();
			init_digital();
			break;
		}
		taskEXIT_CRITICAL();
	}
}

int trxGetNextOrPrevBandFromFrequency(int frequency, bool nextBand)
{
	if (nextBand)
	{
		if (frequency > RADIO_FREQUENCY_BANDS[RADIO_BANDS_TOTAL_NUM - 1].maxFreq)
		{
			return 0; // First band
		}

		for(int band = 0; band < RADIO_BANDS_TOTAL_NUM - 1; band++)
		{
			if (frequency > RADIO_FREQUENCY_BANDS[band].maxFreq && frequency < RADIO_FREQUENCY_BANDS[band + 1].minFreq)
				return (band + 1); // Next band
		}
	}
	else
	{
		if (frequency < RADIO_FREQUENCY_BANDS[0].minFreq)
		{
			return (RADIO_BANDS_TOTAL_NUM - 1); // Last band
		}

		for(int band = 1; band < RADIO_BANDS_TOTAL_NUM; band++)
		{
			if (frequency < RADIO_FREQUENCY_BANDS[band].minFreq && frequency > RADIO_FREQUENCY_BANDS[band - 1].maxFreq)
				return (band - 1); // Prev band
		}
	}

	return -1;
}

int trxGetBandFromFrequency(int frequency)
{
	for(int i = 0; i < RADIO_BANDS_TOTAL_NUM; i++)
	{
		if ((frequency >= RADIO_FREQUENCY_BANDS[i].minFreq) && (frequency <= RADIO_FREQUENCY_BANDS[i].maxFreq))
		{
			return i;
		}
	}

	return -1;
}

bool trxCheckFrequencyInAmateurBand(int tmp_frequency)
{
	return ((tmp_frequency >= BAND_VHF_MIN) && (tmp_frequency <= BAND_VHF_MAX)) ||
			((tmp_frequency >= BAND_UHF_MIN) && (tmp_frequency <= BAND_UHF_MAX)) ||
			((tmp_frequency >= BAND_222_MIN) && (tmp_frequency <= BAND_222_MAX));
}

void trxReadVoxAndMicStrength(void)
{
	taskENTER_CRITICAL();
	read_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x1a, (uint8_t *)&trxTxVox, (uint8_t *)&trxTxMic);
	taskEXIT_CRITICAL();
	}

void trxReadRSSIAndNoise(void)
{
	taskENTER_CRITICAL();
	read_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x1b, (uint8_t *)&trxRxSignal, (uint8_t *)&trxRxNoise);
	taskEXIT_CRITICAL();
}

bool trxCarrierDetected(void)
{
	uint8_t squelch = 0;

	trxReadRSSIAndNoise();

	switch(currentMode)
	{
		case RADIO_MODE_NONE:
			break;

		case RADIO_MODE_ANALOG:
			if (currentChannelData->sql != 0)
			{
				squelch =  TRX_SQUELCH_MAX - (((currentChannelData->sql-1)*11)>>2);
			}
			else
			{
				squelch =  TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]])*11)>>2);
			}
			break;

		case RADIO_MODE_DIGITAL:
			// Don't check for variable squelch, as some people seem to have this set to fully open on their DMR channels.
			/*
			if (currentChannelData->sql!=0)
			{
				squelch =  TRX_SQUELCH_MAX - (((currentChannelData->sql-1)*11)>>2);
			}
			else*/
			{
				squelch =  TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]])*11)>>2);
			}
			break;
	}

	return (trxRxNoise < squelch);
}

void trxCheckDigitalSquelch(void)
{
	trx_measure_count++;
	if (trx_measure_count==25)
	{
		uint8_t squelch;

		trxReadRSSIAndNoise();


		// Don't check for variable squelch, as some people seem to have this set to fully open on their DMR channels.
		/*
		if (currentChannelData->sql!=0)
		{
			squelch =  TRX_SQUELCH_MAX - (((currentChannelData->sql-1)*11)>>2);
		}
		else*/
		{
			squelch =  TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]])*11)>>2);
		}

		if (trxRxNoise < squelch)
		{
			if(!digitalSignalReceived)
			{
				digitalSignalReceived = true;
				GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 1);
			}
		}
		else
		{
			if (digitalSignalReceived)
			{
				digitalSignalReceived = false;
				GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
			}
		}

		trx_measure_count=0;
	}
}

void trxCheckAnalogSquelch(void)
{
	trx_measure_count++;
	if (trx_measure_count==25)
	{
		uint8_t squelch;//=45;

		trxReadRSSIAndNoise();

		// check for variable squelch control
		if (currentChannelData->sql!=0)
		{
			squelch =  TRX_SQUELCH_MAX - (((currentChannelData->sql-1)*11)>>2);
		}
		else
		{
			squelch =  TRX_SQUELCH_MAX - (((nonVolatileSettings.squelchDefaults[trxCurrentBand[TRX_RX_FREQ_BAND]])*11)>>2);
		}

		if (trxRxNoise < squelch)
		{
			if(!analogSignalReceived)
			{
				analogSignalReceived = true;
				displayLightTrigger();
				GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 1);
			}
		}
		else
		{
			if(analogSignalReceived)
			{
				analogSignalReceived=false;
				GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
			}
		}



		if((trxRxNoise < squelch) && (((rxCTCSSactive) && (trxCheckCTCSSFlag())) || (!rxCTCSSactive)))
		{
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1);// Set the audio path to AT1846 -> audio amp.
			enableAudioAmp(AUDIO_AMP_MODE_RF);
		}
		else
		{

			if (trxIsTransmittingTone == false)
			{
				disableAudioAmp(AUDIO_AMP_MODE_RF);
			}
		}

    	trx_measure_count=0;
	}
}

void trxSetFrequency(int fRx,int fTx, int dmrMode)
{
	taskENTER_CRITICAL();
	if (currentRxFrequency!=fRx || currentTxFrequency!=fTx)
	{
		trxCurrentBand[TRX_RX_FREQ_BAND] = trxGetBandFromFrequency(fRx);
		trxCurrentBand[TRX_TX_FREQ_BAND] = trxGetBandFromFrequency(fTx);

		calibrationGetPowerForFrequency(fTx, &trxPowerSettings);
		trxSetPowerFromLevel(nonVolatileSettings.txPowerLevel);



		currentRxFrequency=fRx;
		currentTxFrequency=fTx;

		if (dmrMode==DMR_MODE_AUTO)
		{
			// Most DMR radios determine whether to use Active or Passive DMR depending on whether the Tx and Rx freq are the same
			// This prevents split simplex operation, but since no other radio appears to support split freq simplex
			// Its easier to do things the same way as othe radios, and revisit this again in the future if split freq simplex is required.
			if (currentRxFrequency == currentTxFrequency)
			{
				trxDMRMode = DMR_MODE_ACTIVE;
			}
			else
			{
				trxDMRMode = DMR_MODE_PASSIVE;
			}
		}
		else
		{
			trxDMRMode = dmrMode;
		}

		uint32_t f = currentRxFrequency * 0.16f;
		rx_fl_l = (f & 0x000000ff) >> 0;
		rx_fl_h = (f & 0x0000ff00) >> 8;
		rx_fh_l = (f & 0x00ff0000) >> 16;
		rx_fh_h = (f & 0xff000000) >> 24;

		f = currentTxFrequency * 0.16f;
		tx_fl_l = (f & 0x000000ff) >> 0;
		tx_fl_h = (f & 0x0000ff00) >> 8;
		tx_fh_l = (f & 0x00ff0000) >> 16;
		tx_fh_h = (f & 0xff000000) >> 24;

		if (currentMode==RADIO_MODE_DIGITAL)
		{
			terminate_digital();
		}

		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x70, 0x06 | squelch); // RX off
		}
		else
		{
			// 12.5 kHz settings
			write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x40, 0x06 | squelch); // RX off
		}
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x05, 0x87, 0x63); // select 'normal' frequency mode

		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x29, rx_fh_h, rx_fh_l);
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x2a, rx_fl_h, rx_fl_l);
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x49, 0x0C, 0x15); // setting SQ open and shut threshold

		if (currentBandWidthIs25kHz)
		{
			// 25 kHz settings
			write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x70, 0x26 | squelch); // RX on
		}
		else
		{
			// 12.5 kHz settings
			write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x40, 0x26 | squelch); // RX on
		}

		trxUpdateC6000Calibration();
		trxUpdateAT1846SCalibration();

		if (!txPAEnabled)
		{
			if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_VHF)
			{
				GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);
				GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);
			}
			else
			{
				GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);
				GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);
			}
		}
		else
		{
			//SEGGER_RTT_printf(0, "ERROR Cant enable Rx when PA active\n");
		}

		if (currentMode==RADIO_MODE_DIGITAL)
		{
			init_digital();
		}
	}
	taskEXIT_CRITICAL();
}

int trxGetFrequency(void)
{
	if (trxIsTransmitting)
	{
		return currentTxFrequency;
	}
	else
	{
		return currentRxFrequency;
	}
}

void trx_setRX(void)
{
//	set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0x1F, 0x00, 0x00);
	if (currentMode == RADIO_MODE_ANALOG)
	{
		trxActivateRx();
	}

}

void trx_setTX(void)
{
	trxIsTransmitting = true;

	// RX pre-amp off
	GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);
	GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);

	//	set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0x1F, 0x00, 0x00);
	if (currentMode == RADIO_MODE_ANALOG)
	{
		trxActivateTx();
	}

}

void trxAT1846RxOff(void)
{
	set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0xDF, 0x00, 0x00);
}

void trxAT1846RxOn(void)
{
	set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0xFF, 0x00, 0x20);
}

void trxActivateRx(void)
{
	//SEGGER_RTT_printf(0, "trx_activateRx\n");
    DAC_SetBufferValue(DAC0, 0U, 0U);// PA drive power to zero

    // Possibly quicker to turn them both off, than to check which on is on and turn that one off
	GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 0);// VHF PA off
	GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 0);// UHF PA off

	txPAEnabled=false;

    if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_VHF)
	{
		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 1);// VHF pre-amp on
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 0);// UHF pre-amp off
	}
    else
	{
		GPIO_PinWrite(GPIO_VHF_RX_amp_power, Pin_VHF_RX_amp_power, 0);// VHF pre-amp off
		GPIO_PinWrite(GPIO_UHF_RX_amp_power, Pin_UHF_RX_amp_power, 1);// UHF pre-amp on
	}

	if (currentBandWidthIs25kHz)
	{
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x70, 0x06); 		// 25 kHz settings // RX off
	}
	else
	{
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x40, 0x06); 		// 12.5 kHz settings // RX off
	}

	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x29, rx_fh_h, rx_fh_l);
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x2a, rx_fl_h, rx_fl_l);

	if (currentBandWidthIs25kHz)
	{
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x70, 0x26); // 25 kHz settings // RX on
	}
	else
	{
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x30, 0x40, 0x26); // 12.5 kHz settings // RX on
	}
}

void trxActivateTx(void)
{
	txPAEnabled=true;
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x29, tx_fh_h, tx_fh_l);
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x2a, tx_fl_h, tx_fl_l);

	set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0x1F, 0x00, 0x00); // Clear Tx and Rx bits
	if (currentMode == RADIO_MODE_ANALOG)
	{
		set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0x1F, 0x00, 0x40); // analog TX
		trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_MIC);// For 1750 tone burst
		setMicGainFM(nonVolatileSettings.micGainFM);
	}
	else
	{
		set_clear_I2C_reg_2byte_with_mask(0x30, 0xFF, 0x1F, 0x00, 0xC0); // digital TX
	}

	// TX PA on
	if (trxCurrentBand[TRX_TX_FREQ_BAND] == RADIO_BAND_VHF)
	{
		GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 0);// I can't see why this would be needed. Its probably just for safety.
		GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 1);
	}
	else
	{
		GPIO_PinWrite(GPIO_VHF_TX_amp_power, Pin_VHF_TX_amp_power, 0);// I can't see why this would be needed. Its probably just for safety.
		GPIO_PinWrite(GPIO_UHF_TX_amp_power, Pin_UHF_TX_amp_power, 1);
	}
    DAC_SetBufferValue(DAC0, 0U, txPower);	// PA drive to appropriate level
}

void trxSetPowerFromLevel(int powerLevel)
{
// Note. Fraction values for 200Mhz are currently the same as the VHF band, because there isn't any way to set the 1W value on 220Mhz as there are only 2 calibration tables
//#if//defined(PLATFORM_GD77) || defined(PLATFORM_GD77S) // Support ONLY GD-77 in this build

static const float fractionalPowers[9][10] = {	{0.35,0.39,0.42,0.46,0.50,0.53,0.56,0.72,0.85,0.93},// VHF
												{0.38,0.42,0.45,0.48,0.52,0.55,0.58,0.73,0.86,0.93},// 220Mhz
												{0.41,0.45,0.48,0.51,0.55,0.58,0.64,0.77,0.86,0.93}};// UHF

//#elif defined(PLATFORM_DM1801) //Add extra levels at different time


//static const float fractionalPowers[3][4] = {	{0.28,0.37,0.62,0.82},// VHF
//												{0.28,0.37,0.62,0.82},// 220Mhz
//												{0.05,0.25,0.51,0.75}};// UHF

//#endif
	int stepPerWatt = (trxPowerSettings.highPower - trxPowerSettings.lowPower)/( 5 - 1);

	switch(powerLevel)
	{
		case 0:// 1mW
		case 1:// 5mW
		case 2:// 10mW
		case 3:// 25mW
		case 4:// 50mW
		case 5:// 75mW
		case 6:// 100mW
		case 7:// 250mW
		case 8:// 500mW
		case 9:// 750mW
			txPower = trxPowerSettings.lowPower * fractionalPowers[trxCurrentBand[TRX_TX_FREQ_BAND]][powerLevel];
			break;
		case 10:// 1W
			txPower = trxPowerSettings.lowPower;
			break;
		case 11:// 2W
			txPower = (((powerLevel - 10) * stepPerWatt) * 0.90) + trxPowerSettings.lowPower; // must match number in settings.c
			break;
		case 12:// 3W
			txPower = (((powerLevel - 10) * stepPerWatt) * 0.90) + trxPowerSettings.lowPower;
			break;
		case 13:// 4W
			txPower = (((powerLevel - 10) * stepPerWatt) * 0.90) + trxPowerSettings.lowPower;
			break;
		case 14:// 5W
			txPower = trxPowerSettings.highPower;
			break;
		case 15:// 5W+
			txPower = 4095;
			break;
		default:
			txPower = trxPowerSettings.lowPower;
			break;
	}

	if (txPower>4095)
	{
		txPower=4095;
	}
}

uint16_t trxGetPower(void)
{
	return txPower;
}

void trxCalcBandAndFrequencyOffset(uint32_t *band_offset, uint32_t *freq_offset)
{
// NOTE. For crossband duplex DMR, the calibration potentially needs to be changed every time the Tx/Rx is switched over on each 30ms cycle
// But at the moment this is an unnecessary complication and I'll just use the Rx frequency to get the calibration offsets

	if (trxCurrentBand[TRX_RX_FREQ_BAND] == RADIO_BAND_UHF)
	{
		*band_offset=0x00000000;
		*freq_offset = (currentTxFrequency - 40000000)/1000000;
	}
	else
	{
		*band_offset=0x00000070;
		*freq_offset = (currentTxFrequency - 13250000)/500000;
	}
	// Limit VHF freq calculation exceeds the max lookup table index (of 7)
	if (*freq_offset > 7)
	{
		*freq_offset = 7;
	}
}

void trxUpdateC6000Calibration(void)
{
	uint8_t highByte;
	uint8_t lowByte;
	uint32_t band_offset;
	uint32_t freq_offset;

	if (nonVolatileSettings.useCalibration==false)
	{
		return;
	}

	trxCalcBandAndFrequencyOffset(&band_offset, &freq_offset);

	write_SPI_page_reg_byte_SPI0(0x04, 0x00, 0x3F); // Reset HR-C6000 state

	read_val_DACDATA_shift(band_offset,&lowByte);
	write_SPI_page_reg_byte_SPI0(0x04, 0x37, lowByte); // DACDATA shift (LIN_VOL)

	read_val_Q_MOD2_offset(band_offset,&lowByte);
	write_SPI_page_reg_byte_SPI0(0x04, 0x04, lowByte); // MOD2 offset

	read_val_phase_reduce(band_offset+freq_offset,&lowByte);
	write_SPI_page_reg_byte_SPI0(0x04, 0x46, lowByte); // phase reduce

	read_val_twopoint_mod(band_offset,&lowByte, &highByte);
	uint16_t refOscOffset = (highByte<<8)+lowByte;
	if (refOscOffset>1023)
	{
		refOscOffset=1023;
	}
	write_SPI_page_reg_byte_SPI0(0x04, 0x48, (refOscOffset>>8) & 0x03); // bit 0 to 1 = upper 2 bits of 10-bit twopoint mod
	write_SPI_page_reg_byte_SPI0(0x04, 0x47, (refOscOffset & 0xFF)); // bit 0 to 7 = lower 8 bits of 10-bit twopoint mod
}

void I2C_AT1846_set_register_with_mask(uint8_t reg, uint16_t mask, uint16_t value, uint8_t shift)
{
	set_clear_I2C_reg_2byte_with_mask(reg, (mask & 0xff00) >> 8, (mask & 0x00ff) >> 0, ((value << shift) & 0xff00) >> 8, ((value << shift) & 0x00ff) >> 0);
}

void trxUpdateAT1846SCalibration(void)
{
	uint32_t band_offset=0x00000000;
	uint32_t freq_offset=0x00000000;

	if (nonVolatileSettings.useCalibration==false)
	{
		return;
	}

	trxCalcBandAndFrequencyOffset(&band_offset, &freq_offset);

	uint8_t val_pga_gain;
	uint8_t gain_tx;
	uint8_t padrv_ibit;

	uint16_t xmitter_dev;

	uint8_t dac_vgain_analog;
	uint8_t volume_analog;

	uint16_t noise1_th;
	uint16_t noise2_th;
	uint16_t rssi3_th;

	uint16_t squelch_th;

	read_val_pga_gain(band_offset, &val_pga_gain);
	read_val_voice_gain_tx(band_offset, &voice_gain_tx);
	read_val_gain_tx(band_offset, &gain_tx);
	read_val_padrv_ibit(band_offset, &padrv_ibit);

	if (currentBandWidthIs25kHz)
	{
		// 25 kHz settings
		read_val_xmitter_dev_wideband(band_offset, &xmitter_dev);
	}
	else
	{
		// 12.5 kHz settings
		read_val_xmitter_dev_narrowband(band_offset, &xmitter_dev);
	}

	if (currentMode == RADIO_MODE_ANALOG)
	{
		read_val_dac_vgain_analog(band_offset, &dac_vgain_analog);
		read_val_volume_analog(band_offset, &volume_analog);
	}
	else
	{
		dac_vgain_analog = 0x0C;
		volume_analog = 0x0C;
	}

	if (currentBandWidthIs25kHz)
	{
		// 25 kHz settings
		read_val_noise1_th_wideband(band_offset, &noise1_th);
		read_val_noise2_th_wideband(band_offset, &noise2_th);
		read_val_rssi3_th_wideband(band_offset, &rssi3_th);

		read_val_squelch_th(band_offset+freq_offset, 0, &squelch_th);
	}
	else
	{
		// 12.5 kHz settings
		read_val_noise1_th_narrowband(band_offset, &noise1_th);
		read_val_noise2_th_narrowband(band_offset, &noise2_th);
		read_val_rssi3_th_narrowband(band_offset, &rssi3_th);

		read_val_squelch_th(band_offset+freq_offset, 3, &squelch_th);
	}

	I2C_AT1846_set_register_with_mask(0x0A, 0xF83F, val_pga_gain, 6);
	I2C_AT1846_set_register_with_mask(0x41, 0xFF80, voice_gain_tx, 0);
	I2C_AT1846_set_register_with_mask(0x44, 0xF0FF, gain_tx, 8);

	I2C_AT1846_set_register_with_mask(0x59, 0x003f, xmitter_dev, 6);
	I2C_AT1846_set_register_with_mask(0x44, 0xFF0F, dac_vgain_analog, 4);
	I2C_AT1846_set_register_with_mask(0x44, 0xFFF0, volume_analog, 0);

	I2C_AT1846_set_register_with_mask(0x48, 0x0000, noise1_th, 0);
	I2C_AT1846_set_register_with_mask(0x60, 0x0000, noise2_th, 0);
	I2C_AT1846_set_register_with_mask(0x3f, 0x0000, rssi3_th, 0);

	I2C_AT1846_set_register_with_mask(0x0A, 0x87FF, padrv_ibit, 11);

	I2C_AT1846_set_register_with_mask(0x49, 0x0000, squelch_th, 0);
}

void trxSetDMRColourCode(int colourCode)
{
	write_SPI_page_reg_byte_SPI0(0x04, 0x1F, (colourCode << 4)); // DMR Colour code in upper 4 bits.
	currentCC = colourCode;
}

int trxGetDMRColourCode(void)
{
	return currentCC;
}

int trxGetDMRTimeSlot(void)
{
	return trxCurrentDMRTimeSlot;
	//return ((currentChannelData->flag2 & 0x40)!=0);
}

void trxSetDMRTimeSlot(int timeslot)
{
	trxCurrentDMRTimeSlot = timeslot;
}

void trxUpdateTsForCurrentChannelWithSpecifiedContact(struct_codeplugContact_t *contactData)
{
	bool hasManualTsOverride = false;

	// nonVolatileSettings.tsManualOverride stores separate TS overrides for VFO and Channel mode
	// Lower nibble is the Channel screen override and upper nibble if the VFO
	if (nonVolatileSettings.initialMenuNumber == UI_CHANNEL_MODE)
	{
		if ((nonVolatileSettings.tsManualOverride & 0x0F) != 0)
		{
			hasManualTsOverride = true;
		}
	}
	else
	{
		if ((nonVolatileSettings.tsManualOverride & 0xF0) != 0)
		{
			hasManualTsOverride = true;
		}
	}

	if (!hasManualTsOverride)
	{
		if ((contactData->reserve1 & 0x01) == 0x00)
		{
			if ( (contactData->reserve1 & 0x02) !=0 )
			{
				trxCurrentDMRTimeSlot = 1;
			}
			else
			{
				trxCurrentDMRTimeSlot = 0;
			}
		}
		else
		{
			trxCurrentDMRTimeSlot = ((currentChannelData->flag2 & 0x40)!=0);
		}
	}
}

void trxSetTxCTCSS(int toneFreqX10)
{
	taskENTER_CRITICAL();
	if (!codeplugChannelToneIsCTCSS(toneFreqX10))
	{
		// tone value of 0xffff in the codeplug seem to be a flag that no tone has been selected
        write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x4a, 0x00,0x00); //Zero the CTCSS1 Register
		set_clear_I2C_reg_2byte_with_mask(0x4e,0xF9,0xFF,0x00,0x00);    //disable the transmit CTCSS
	}
	else
	{
		toneFreqX10 = toneFreqX10*10;// value that is stored is 100 time the tone freq but its stored in the codeplug as freq times 10
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT,	0x4a, (toneFreqX10 >> 8) & 0xff,	(toneFreqX10 & 0xff));
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT,	0x4b, 0x00, 0x00); // init cdcss_code
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT,	0x4c, 0x0A, 0xE3); // init cdcss_code
		set_clear_I2C_reg_2byte_with_mask(0x4e,0xF9,0xFF,0x06,0x00);    //enable the transmit CTCSS
	}
	taskEXIT_CRITICAL();
}

void trxSetRxCTCSS(int toneFreqX10)
{
	taskENTER_CRITICAL();
	if (!codeplugChannelToneIsCTCSS(toneFreqX10))
	{
		// tone value of 0xffff in the codeplug seem to be a flag that no tone has been selected
        write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x4d, 0x00,0x00); //Zero the CTCSS2 Register
        rxCTCSSactive=false;
	}
	else
	{
		int threshold=(2500-toneFreqX10)/100;   //adjust threshold value to match tone frequency.
		if(toneFreqX10>2400) threshold=1;
		toneFreqX10 = toneFreqX10*10;// value that is stored is 100 time the tone freq but its stored in the codeplug as freq times 10
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT,	0x4d, (toneFreqX10 >> 8) & 0xff,	(toneFreqX10 & 0xff));
		write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x5b,(threshold & 0xFF),(threshold & 0xFF)); //set the detection thresholds
		set_clear_I2C_reg_2byte_with_mask(0x3a,0xFF,0xE0,0x00,0x08);    //set detection to CTCSS2
		rxCTCSSactive=true;
	}
	taskEXIT_CRITICAL();
}

bool trxCheckCTCSSFlag(void)
{
	uint8_t FlagsH;
	uint8_t FlagsL;

	taskENTER_CRITICAL();
	read_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x1c,&FlagsH,&FlagsL);
	taskEXIT_CRITICAL();
	return (FlagsH & 0x01);
}

void trxUpdateDeviation(int channel)
{
	uint8_t deviation;

	if (nonVolatileSettings.useCalibration == false)
	{
		return;
	}

	taskENTER_CRITICAL();
	switch (channel)
	{
	case AT1846_VOICE_CHANNEL_TONE1:
	case AT1846_VOICE_CHANNEL_TONE2:
		read_val_dev_tone(CAL_DEV_TONE, &deviation);
		deviation &= 0x7f;
		I2C_AT1846_set_register_with_mask(0x59, 0x003f, deviation, 6); // Tone deviation value
		break;
	case AT1846_VOICE_CHANNEL_DTMF:
		read_val_dev_tone(CAL_DEV_DTMF, &deviation);
		deviation &= 0x7f;
		I2C_AT1846_set_register_with_mask(0x59, 0x003f, deviation, 6); // Tone deviation value
		break;
	}
	taskEXIT_CRITICAL();
}

uint8_t trxGetCalibrationVoiceGainTx(void)
{
	return voice_gain_tx;
}

void trxSelectVoiceChannel(uint8_t channel) {
	uint8_t valh;
	uint8_t vall;

	taskENTER_CRITICAL();
	switch (channel)
	{
	case AT1846_VOICE_CHANNEL_TONE1:
	case AT1846_VOICE_CHANNEL_TONE2:
	case AT1846_VOICE_CHANNEL_DTMF:
		set_clear_I2C_reg_2byte_with_mask(0x79, 0xff, 0xff, 0xc0, 0x00); // Select single tone
		set_clear_I2C_reg_2byte_with_mask(0x57, 0xff, 0xfe, 0x00, 0x01); // Audio feedback on

		read_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x41, &valh, &trxSaveVoiceGainTx);
		trxSaveVoiceGainTx &= 0x7f;

		read_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x59, &valh, &vall);
		trxSaveDeviation = (vall + (valh<<8)) >> 6;

		trxUpdateDeviation(channel);

		set_clear_I2C_reg_2byte_with_mask(0x41, 0xff, 0x80, 0x00, 0x05);
		break;
	default:
		set_clear_I2C_reg_2byte_with_mask(0x57, 0xff, 0xfe, 0x00, 0x00); // Audio feedback off
		if (trxSaveVoiceGainTx != 0xff)
		{
			I2C_AT1846_set_register_with_mask(0x41, 0xFF80, trxSaveVoiceGainTx, 0);
		}
		if (trxSaveDeviation != 0xFF)
		{
			I2C_AT1846_set_register_with_mask(0x59, 0x003f, trxSaveDeviation, 6);
		}
		break;
	}
	set_clear_I2C_reg_2byte_with_mask(0x3a, 0x8f, 0xff, channel, 0x00);
	taskEXIT_CRITICAL();
}

void trxSetTone1(int toneFreq)
{

	toneFreq = toneFreq * 10;
	taskENTER_CRITICAL();
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x35, (toneFreq >> 8) & 0xff,	(toneFreq & 0xff));   // tone1_freq
	taskEXIT_CRITICAL();
}

void trxSetTone2(int toneFreq)
{
	toneFreq = toneFreq * 10;
	taskENTER_CRITICAL();
	write_I2C_reg_2byte(I2C_MASTER_SLAVE_ADDR_7BIT, 0x36, (toneFreq >> 8) & 0xff,	(toneFreq & 0xff));   // tone2_freq
	taskEXIT_CRITICAL();
}

void trxSetDTMF(int code)
{
	if (code < 16)
	{
		trxSetTone1(trxDTMFfreq1[code]);
		trxSetTone2(trxDTMFfreq2[code]);
	}
}