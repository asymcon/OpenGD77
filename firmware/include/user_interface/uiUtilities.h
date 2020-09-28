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
#ifndef _MENU_UTILITY_QSO_DATA_H_
#define _MENU_UTILITY_QSO_DATA_H_                    /**< Symbol preventing repeated inclusion */
#include <user_interface/menuSystem.h>
#include <functions/settings.h>


#define MAX_ZONE_SCAN_NUISANCE_CHANNELS       16
#define NUM_LASTHEARD_STORED                  32

#define QSO_TIMER_TIMEOUT                   2400

#if defined(PLATFORM_RD5R)
#define TX_TIMER_Y_OFFSET                     12
#define CONTACT_Y_POS                         12
#define CONTACT_FIRST_LINE_Y_POS              24
#define CONTACT_SECOND_LINE_Y_POS             33
#define SQUELCH_BAR_Y_POS                     14
#define SQUELCH_BAR_H                          4
#else
#define TX_TIMER_Y_OFFSET                      8
#define CONTACT_Y_POS                         16
#define CONTACT_FIRST_LINE_Y_POS              32
#define CONTACT_SECOND_LINE_Y_POS             48
#define SQUELCH_BAR_Y_POS                     16
#define SQUELCH_BAR_H                          9
#endif

#define FREQUENCY_X_POS  /* '>Ta'*/ ((3 * 8) + 4)
#if defined(PLATFORM_GD77) || defined(PLATFORM_GD77S)
#define MAX_POWER_SETTING_NUM                  18
#else
#define MAX_POWER_SETTING_NUM                  9
#endif
#define NUM_PC_OR_TG_DIGITS                    8
#define MAX_TG_OR_PC_VALUE              16777215

#if defined(PLATFORM_GD77S)
#define ANNOUNCE_STATIC
#else
#define ANNOUNCE_STATIC static
#endif

#if defined(PLATFORM_GD77S)
// Displaying ANNOUNCE_STATIC here is a bit overkill but it makes things clearer.
ANNOUNCE_STATIC void announceRadioMode(bool voicePromptWasPlaying);
ANNOUNCE_STATIC void announceZoneName(bool voicePromptWasPlaying);
ANNOUNCE_STATIC void announceContactNameTgOrPc(void);
ANNOUNCE_STATIC void announcePowerLevel(void);
ANNOUNCE_STATIC void announceBatteryPercentage(void);
ANNOUNCE_STATIC void announceTS(void);
ANNOUNCE_STATIC void announceCC(void);
ANNOUNCE_STATIC void announceChannelName(bool voicePromptWasPlaying);
ANNOUNCE_STATIC void announceFrequency(void);
ANNOUNCE_STATIC void announceVFOAndFrequency(void);
#endif

extern struct_codeplugRxGroup_t currentRxGroupData;
extern struct_codeplugContact_t currentContactData;
extern struct_codeplugZone_t currentZone;

enum UI_CALL_STATE { NOT_IN_CALL=0, PRIVATE_CALL_ACCEPT, PRIVATE_CALL, PRIVATE_CALL_DECLINED };

typedef enum
{
	PROMPT_SEQUENCE_CHANNEL_NAME_OR_VFO_FREQ,
	PROMPT_SEQUENCE_ZONE,
	PROMPT_SEQUENCE_MODE,
	PROMPT_SEQUENCE_CONTACT_TG_OR_PC,
	PROMPT_SEQUENCE_TS,
	PROMPT_SEQUENCE_CC,
	PROMPT_SEQUENCE_POWER,
	PROMPT_SEQUENCE_BATTERY,
	PROMPT_SQUENCE_SQUELCH,
	NUM_PROMPT_SEQUENCES
} voicePromptItem_t;

extern voicePromptItem_t voicePromptSequenceState;

typedef struct dmrIdDataStruct
{
	int id;
	char text[20];
} dmrIdDataStruct_t;

#define MIN_ENTRIES_BEFORE_USING_SLICES 40 // Minimal number of available IDs before using slices stuff
#define ID_SLICES 14 // Number of slices in whole DMRIDs DB

typedef struct
{
	uint32_t entries;
	uint8_t  contactLength;
	int32_t  slices[ID_SLICES]; // [0] is min availabel ID, [ID_SLICES - 1] is max available ID
	uint32_t IDsPerSlice;

} dmrIDsCache_t;

typedef struct LinkItem
{
    struct LinkItem *prev;
    uint32_t 	id;
    uint32_t 	talkGroupOrPcId;
    char        contact[21];
    char        talkgroup[17];
    char 		talkerAlias[32];// 4 blocks of data. 6 bytes + 7 bytes + 7 bytes + 7 bytes . plus 1 for termination some more for safety.
    char 		locator[7];
    uint32_t	time;// current system time when this station was heard
    struct LinkItem *next;
} LinkItem_t;

enum QSO_DISPLAY_STATE { QSO_DISPLAY_IDLE, QSO_DISPLAY_DEFAULT_SCREEN, QSO_DISPLAY_CALLER_DATA, QSO_DISPLAY_CALLER_DATA_UPDATE };

typedef enum
{
	SCAN_SCANNING = 0,
	SCAN_SHORT_PAUSED,
	SCAN_PAUSED
} ScanState_t;

extern const char *POWER_LEVELS[];
extern const char *POWER_LEVEL_UNITS[];
extern const char *DMR_DESTINATION_FILTER_LEVELS[];
extern const char *DMR_CCTS_FILTER_LEVELS[];
extern const char *ANALOG_FILTER_LEVELS[];
extern const int SCAN_SHORT_PAUSE_TIME;			//time to wait after carrier detected to allow time for full signal detection. (CTCSS or DMR)
extern const int SCAN_TOTAL_INTERVAL;			    //time between each scan step
extern const int SCAN_DMR_SIMPLEX_MIN_INTERVAL;		//minimum time between steps when scanning DMR Simplex. (needs extra time to capture TDMA Pulsing)
extern const int SCAN_FREQ_CHANGE_SETTLING_INTERVAL;//Time after frequency is changed before RSSI sampling starts
extern const int SCAN_SKIP_CHANNEL_INTERVAL;		//This is actually just an implicit flag value to indicate the channel should be skipped
extern ScanState_t scanState;
extern int scanTimer;
extern bool scanActive;
extern bool scanToneActive;
extern bool displaySquelch;
extern int scanDirection;


extern LinkItem_t *LinkHead;
extern int menuDisplayQSODataState;
extern int qsodata_timer;
extern uint32_t menuUtilityReceivedPcId;
extern uint32_t menuUtilityTgBeforePcMode;
extern const uint32_t RSSI_UPDATE_COUNTER_RELOAD;
extern settingsStruct_t originalNonVolatileSettings; // used to store previous settings in options edition related menus.
extern int nuisanceDelete[MAX_ZONE_SCAN_NUISANCE_CHANNELS];
extern int nuisanceDeleteIndex;
extern char freq_enter_digits[12];
extern int freq_enter_idx;
extern int numLastHeard;
extern bool inhibitInitialVoicePrompt;
extern int tmpQuickMenuDmrDestinationFilterLevel;
extern int tmpQuickMenuDmrCcTsFilterLevel;
extern int tmpQuickMenuAnalogFilterLevel;

#define TS_NO_OVERRIDE  0
bool tsIsOverridden(Channel_t chan);
int8_t tsGetOverride(Channel_t chan);
void tsSetOverride(Channel_t chan, int8_t ts);
void tsSetContactOverride(Channel_t chan, struct_codeplugContact_t *contact);


bool isQSODataAvailableForCurrentTalker(void);
char *chomp(char *str);
int32_t getFirstSpacePos(char *str);
void dmrIDCacheInit(void);
bool dmrIDLookup(int targetId, dmrIdDataStruct_t *foundRecord);
bool contactIDLookup(uint32_t id, int calltype, char *buffer);
void menuUtilityRenderQSOData(void);
void menuUtilityRenderHeader(void);
LinkItem_t *lastheardFindInList(uint32_t id);
void lastheardInitList(void);
bool lastHeardListUpdate(uint8_t *dmrDataBuffer, bool forceOnHotspot);
void lastHeardClearLastID(void);
void drawRSSIBarGraph(void);
void drawFMMicLevelBarGraph(void);
void drawDMRMicLevelBarGraph(void);
void setOverrideTGorPC(int tgOrPc, bool privateCall);
void printFrequency(bool isTX, bool hasFocus, uint8_t y, uint32_t frequency, bool displayVFOChannel, bool isScanMode);
void printToneAndSquelch(void);
size_t snprintDCS(char *s, size_t n, uint16_t code, bool inverted);
void reset_freq_enter_digits(void);
int read_freq_enter_digits(int startDigit, int endDigit);
int getBatteryPercentage(void);
void decreasePowerLevel(void);
void increasePowerLevel(void);

void announceChar(char ch);
void announceCSSCode(uint16_t code, CSSTypes_t cssType, bool inverted);

void announceItem(voicePromptItem_t item, audioPromptThreshold_t immediateAnnouceThreshold);
void playNextSettingSequence(void);
void buildTgOrPCDisplayName(char *nameBuf, int bufferLen);
void acceptPrivateCall(int id );
bool repeatVoicePromptOnSK1(uiEvent_t *ev);


#endif
