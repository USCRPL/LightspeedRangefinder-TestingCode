//
// Test program that streams data using infinite length mode, matching the rangefinder's RF settings.
//

#include <mbed.h>
#include <SerialStream.h>

#include <CC1200.h>
#include <cinttypes>

#include "../pins.h"
#include <CC1200Morse.h>

#include "RadioSettingsMenu.h"


BufferedSerial serial(USBTX, USBRX, 115200);
SerialStream<BufferedSerial> pc(serial);

CC1200 radio(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_CS, PIN_RADIO_RST, &pc);
CC1200 dummy(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_DUMMY_CS, PIN_RADIO_DUMMY_RST, &pc);

const auto onTime = 1s;
const auto offTime = 4s;

void configureRFSettings()
{
	askForRadioSettings(pc, radio);

	radio.setPacketMode(CC1200::PacketMode::FIXED_LENGTH);
	radio.setCRCEnabled(false);

	// set frequency
	radio.setSymbolRate(8.0f / chrono::duration_cast<chrono::duration<float>>(onTime).count());

	// disable anything getting sent before the data
	radio.configureSyncWord(0x0, CC1200::SyncMode::SYNC_NONE, 8);
	radio.configurePreamble(0, 0);

	// configure OOK modulation
	radio.setModulationFormat(CC1200::ModFormat::ASK);
	radio.disablePARamping();

	// 1 byte packet
	radio.setPacketLength(1);
}

int main()
{
	pc.printf(">> Configuring radio...\n");
	if(!radio.begin())
	{
		pc.printf("ERROR: Failed to connect to CC1200\n");
		while(true){}
	}

	configureRFSettings();

	while (true)
	{
		char data = 0xFF;
		radio.enqueuePacket(&data, 1);
		radio.startTX();

		while(radio.getTXFIFOLen() > 0)
		{
			pc.printf("TX radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, FS lock = 0x%u\n",
					  static_cast<uint8_t>(radio.getState()), radio.getTXFIFOLen(), radio.readRegister(CC1200::ExtRegister::FSCAL_CTRL) & 1);

			//ThisThread::sleep_for(5ms);
		}

		ThisThread::sleep_for(onTime);

		radio.idle();

		ThisThread::sleep_for(offTime);
	}

}