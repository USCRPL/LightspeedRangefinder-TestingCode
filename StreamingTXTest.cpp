//
// Test program that streams data using infinite length mode, matching the rangefinder's RF settings.
//

#include <mbed.h>
#include <SerialStream.h>

#include <CC1200.h>
#include <cinttypes>

#include "../pins.h"
#include "RadioSettingsMenu.h"


BufferedSerial serial(USBTX, USBRX, 115200);
SerialStream<BufferedSerial> pc(serial);

CC1200 radio(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_CS, PIN_RADIO_RST, &pc);
CC1200 dummy(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_DUMMY_CS, PIN_RADIO_DUMMY_RST, &pc);

void configureRFSettings()
{
	askForRadioSettings(pc, radio);

	// Specific to this test suite: enable infinite length mode
	radio.setPacketMode(CC1200::PacketMode::INFINITE_LENGTH, false);
}

const size_t dataLen = 128; // Size of TX FIFO
char testData[dataLen];

const size_t transmissionLen = dataLen*10;

/**
 * Transmit data for a while, then shut down.
 */
void transmitStream()
{
	pc.printf(">> Starting transmission...\n");

	// fill buffer with initial data
	size_t lengthWritten = radio.writeStream(testData, dataLen);
	if(lengthWritten != dataLen)
	{
		pc.printf("Error: FIFO didn't fill all the way, only wrote %zu bytes\n", lengthWritten);
	}

	// Activate transmit mode.  This will send a new sync word for the receiver to key on.
	// This will start streaming data at 500kbps, we need to keep ahead of it.
	// Luckily, our SPI runs at 5Mbps so that should be manageable.
	// However, there can't be any printfs after here or the timing gets disrupted.
	radio.startTX();

	// allow some time for TX mode to activate
	wait_us(500);

	radio.updateState();
	if(radio.getState() != CC1200::State::TX)
	{
		pc.printf("Error: TX did not enable\n");
	}

	// Keep writing the test data as fast as the TX buffer empties.
	size_t successfulBytes = 0;
	Timer timeoutTimer;
	timeoutTimer.start();
	while(true)
	{
		bool txSuccessful = radio.writeStreamBlocking(testData, dataLen);
		if(txSuccessful)
		{
			successfulBytes += dataLen;
		}
		else
		{
			pc.printf(">> ERROR: Radio entered state %" PRIu8 "\n", static_cast<uint8_t>(radio.getState()));
			break;
		}

		if(successfulBytes >= transmissionLen)
		{
			// Signal end of transmission
			char endByte = 0xDD;
			radio.writeStreamBlocking(&endByte, 1);
			break;
		}
	}

	pc.printf("%zu bytes were successfully transmitted.\n", successfulBytes);

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

	// Generate data buffer to send.
	// We want to send an alternating pattern of 0xAAs and 0xBBs, to make sure it's preserved on the other end.
	for(size_t index = 0; index < dataLen; ++index)
	{
		testData[index] = (index % 2 == 0 ? 0xAA : 0xBB);
	}

	while (true)
	{
		transmitStream();

		// wait 1s for amp to cool down
		ThisThread::sleep_for(1s);

		// If all else fails, try hitting it!
		// Ensure TX buffer is clear.
		radio.sendCommand(CC1200::Command::FLUSH_TX);

		// Now go to idle and give the user time to read the message.
		radio.sendCommand(CC1200::Command::IDLE);
	}

}