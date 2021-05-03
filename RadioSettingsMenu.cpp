//
// Created by jamie on 2/19/2021.
//

#include "RadioSettingsMenu.h"

void flightConfiguration(CC1200 & radio)
{
	// Radio settings (found through looooots of testing)
	radio.configureFIFOMode();
	radio.setModulationFormat(CC1200::ModFormat::FSK_2);
	radio.setFSKDeviation(124816);
	radio.setSymbolRate(500000);
	radio.setRadioFrequency(CC1200::Band::BAND_410_480MHz, 442000000);
	radio.configureDCFilter(true, 1, 4);
	radio.setRXFilterBandwidth(833333, false);
	radio.setIFCfg(CC1200::IFCfg::POSITIVE_DIV_6, false);
	radio.configureSyncWord(0x930B51DE, CC1200::SyncMode::SYNC_32_BITS, 8); // default sync word, for now
	radio.configurePreamble(5, 0); // default preamble setting
	radio.setPacketMode(CC1200::PacketMode::VARIABLE_LENGTH, true);
	radio.setCRCEnabled(true);
	radio.setPARampRate(2, 5, CC1200::RampTime::RAMP_3_SYMBOL); // default PA setting

	// AGC configuration (straight from SmartRF)
	radio.setAGCReferenceLevel(0x2F);
	radio.setAGCSyncBehavior(CC1200::SyncBehavior::FREEZE_NONE);

	// enable all AGC steps for NORMAL mode
	radio.setAGCGainTable(CC1200::GainTable::NORMAL, 17, 0);

	radio.setAGCHysteresis(0b10);
	radio.setAGCSlewRate(0);
	radio.setAGCSettleWait(0x6);

	// Calibrate FS when switching to transmit mode (other options don't seem to work)
	radio.setFSCalMode(CC1200::FSCalMode::FROM_IDLE);

	// Configure GPIO2 to generate the interrupt
	radio.configureGPIO(2, CC1200::GPIOMode::PKT_SYNC_RXTX);
}

void askForRadioSettings(Stream& pc, CC1200 &radio)
{
	int config=-1;
	//MENU. ADD AN OPTION FOR EACH TEST.
	pc.printf("Select a config: \n");
	pc.printf("1.  38.4ksps 2-GFSK DEV=20kHz CHF=104kHz\n");
	pc.printf("2.  38.4ksps 2-GFSK DEV=20kHz CHF=104kHz Max-IF\n");
	pc.printf("3.  38.4ksps 2-GFSK DEV=20kHz CHF=104kHz Zero-IF\n");
    pc.printf("4.  100ksps 2-GFSK DEV=50kHz CHF=208kHz\n");
    pc.printf("5.  100ksps 2-GFSK DEV=50kHz CHF=208kHz Max-IF\n");
    pc.printf("6.  100ksps 2-GFSK DEV=50kHz CHF=208kHz Zero-IF\n");
    pc.printf("7.  500ksps 2-FSK DEV=125kHz CHF=833kHz\n");
    pc.printf("8.  500ksps 2-FSK DEV=125kHz CHF=833kHz Max-IF\n");
    pc.printf("9.  500ksps 2-FSK DEV=399kHz CHF=1666kHz Zero-IF\n");
	pc.printf("10.  Flight Configuration\n");

	config = 0;
	pc.scanf("%d", &config);
	pc.printf("Running test with config %d:\n\n", config);

	CC1200::Band band = CC1200::Band::BAND_410_480MHz;
	float frequency = 442e6;
	float fskDeviation;
	float symbolRate;
	float rxFilterBW;
	CC1200::ModFormat modFormat;

	CC1200::IFCfg ifCfg = CC1200::IFCfg::POSITIVE_DIV_4;

	bool dcOffsetCorrEnabled = true;
	uint8_t agcRefLevel = 0x2A;
	uint8_t agcSettleWait = 0x2;

	uint8_t dcFiltSettlingCfg = 1; // default chip setting
	uint8_t dcFiltCutoffCfg = 4; // default chip setting

	CC1200::SyncMode syncMode = CC1200::SyncMode::SYNC_32_BITS;
	uint32_t syncWord = 0x930B51DE; // default sync word
	uint8_t syncThreshold = 8; // TI seems to recommend this threshold for most configs above 100ksps

	uint8_t preableLengthCfg = 5; // default chip setting
	uint8_t preambleFormatCfg = 0; // default chip setting

	if(config >= 1 && config <= 9)
	{
		fskDeviation = 19989;
		symbolRate = 38400;
		rxFilterBW = 104167;
		modFormat = CC1200::ModFormat::GFSK_2;

		if(config == 1)
		{
			ifCfg = CC1200::IFCfg::POSITIVE_DIV_8;
			agcSettleWait = 0x1;
		}
		else if(config == 2)
		{
			ifCfg = CC1200::IFCfg::POSITIVE_DIV_4;
			agcSettleWait = 0x1;
		}
		else if (config == 3)
		{
			ifCfg = CC1200::IFCfg::ZERO;
			agcRefLevel = 0x2F;

			dcFiltSettlingCfg = 3;
			dcFiltCutoffCfg = 6;

		}
		else if (config == 4)
        {
            fskDeviation = 49896;
            symbolRate = 100000;
            rxFilterBW = 208333;
		    ifCfg = CC1200::IFCfg::POSITIVE_DIV_8;
        }
		else if (config == 5)
        {
            fskDeviation = 49896;
            symbolRate = 100000;
            rxFilterBW = 208333;
		    ifCfg = CC1200::IFCfg::POSITIVE_DIV_4;
        }
		else if (config == 6)
        {
            fskDeviation = 49896;
            symbolRate = 100000;
            rxFilterBW = 208333;
		    ifCfg = CC1200::IFCfg::ZERO;

			agcRefLevel = 0x2F;
			dcFiltSettlingCfg = 3;
			dcFiltCutoffCfg = 6;
		}
        else if (config == 7)
        {
            fskDeviation = 124816;
            symbolRate = 500000;
            rxFilterBW = 833333;
            modFormat = CC1200::ModFormat::FSK_2;
            ifCfg = CC1200::IFCfg::POSITIVE_DIV_6;
			agcRefLevel = 0x2F;
			agcSettleWait = 0x6;

		}
        else if (config == 8)
        {
            fskDeviation = 124816;
            symbolRate = 500000;
            rxFilterBW = 833333;
            modFormat = CC1200::ModFormat::FSK_2;
            ifCfg = CC1200::IFCfg::POSITIVE_DIV_4;
			agcRefLevel = 0x2F;
			agcSettleWait = 0x6;
		}
        else
        {
            fskDeviation = 399169;
            symbolRate = 500000;
            rxFilterBW = 1666700;
            modFormat = CC1200::ModFormat::FSK_2;
            ifCfg = CC1200::IFCfg::ZERO;
			agcRefLevel = 0x2F;

			dcFiltSettlingCfg = 3;
			dcFiltCutoffCfg = 6;
		}
	}
	else if(config == 10)
	{
		flightConfiguration(radio);
	}
	else
	{
		pc.printf("Invalid entry.\n");
	}

	radio.configureFIFOMode();
	radio.setPacketMode(CC1200::PacketMode::VARIABLE_LENGTH);
	if(config != 10)
	{
		radio.configureDCFilter(dcOffsetCorrEnabled, dcFiltSettlingCfg, dcFiltCutoffCfg);
		radio.setModulationFormat(modFormat);
		radio.setFSKDeviation(fskDeviation);
		radio.setSymbolRate(symbolRate);
		radio.setRadioFrequency(band, frequency);
		radio.setRXFilterBandwidth(rxFilterBW, false);
		radio.setIFCfg(ifCfg, false);
		radio.configureSyncWord(syncWord, syncMode, syncThreshold);
		radio.configurePreamble(preableLengthCfg, preambleFormatCfg);

		// AGC configuration (straight from SmartRF)
		radio.setAGCReferenceLevel(agcRefLevel);
		radio.setAGCSyncBehavior(CC1200::SyncBehavior::FREEZE_NONE);
		if(ifCfg == CC1200::IFCfg::ZERO)
		{
			radio.setAGCGainTable(CC1200::GainTable::ZERO_IF, 11, 0);
		}
		else
		{
			// enable all AGC steps for NORMAL mode
			radio.setAGCGainTable(CC1200::GainTable::NORMAL, 17, 0);
		}
		radio.setAGCHysteresis(0b10);
		radio.setAGCSlewRate(0);
		radio.setAGCSettleWait(agcSettleWait);
	}

	radio.writeRegister(CC1200::ExtRegister::FS_DIG0,0xA3);

	int boardRevision=-1;
	//MENU. ADD AN OPTION FOR EACH TEST.
	pc.printf("Select board revision: \n");
	pc.printf("1.  RangefinderTest\n");
	pc.printf("2.  Ground Station V1 +31.0dBm\n");
	pc.printf("3.  Ground Station V1 +1.5dBm\n"); // used for receive sensitivity testing
	pc.printf("4.  Transponder V1 +11.1dBm\n");
	pc.printf("5.  Ground Station V2 +33dBm\n");
	pc.printf("6.  Ground Station V2 -0.6 dBm\n");
	pc.printf("7.  Ground Station V2 +23.5 dBm\n");

	pc.scanf("%d", &boardRevision);
	pc.printf("Running test with revision  %d:\n\n", boardRevision);

	if(boardRevision == 1)
	{
		radio.setOutputPower(14); // full output power
		radio.setRSSIOffset(0); // NOT MEASURED
	}
	else if(boardRevision == 2)
	{
		radio.setOutputPower(14); // full output power
		radio.setRSSIOffset(-99); // Calibrated for SKY65366 LNA
	}
	else if(boardRevision == 3)
	{
		radio.setOutputPower(-16); // calibrated for SKY65366
		radio.setRSSIOffset(-99); // Calibrated for SKY65366 LNA
	}
	else if(boardRevision == 4)
	{
		radio.setOutputPower(14);
		//radio.setOutputPower(-10);
		radio.setRSSIOffset(-76); // Calibrated for transponder (no LNA)
		radio.configurePreamble(0b1011, 0);

	}
	else if(boardRevision == 5)
	{
		radio.setOutputPower(0);
		radio.setRSSIOffset(-96); // Calibrated for PSA4 LNA
		radio.configurePreamble(0b1101, 0);
	}
	else if(boardRevision == 6)
	{
		radio.setOutputPower(-16);
		radio.setRSSIOffset(-96); // Calibrated for PSA4 LNA
		radio.configurePreamble(0b1101, 0);
	}
	else if(boardRevision == 7)
	{
		radio.setOutputPower(-7);
		radio.setRSSIOffset(-96); // Calibrated for PSA4 LNA
		radio.configurePreamble(0b1101, 0);
	}
}
