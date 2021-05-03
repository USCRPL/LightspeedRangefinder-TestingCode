//
// Test program receives a stream of data using infinite length mode, matching the rangefinder's RF settings.
//

#include <mbed.h>
#include <SerialStream.h>

#include <CC1200.h>
#include <cinttypes>

#include "../MovingAverage.h"
#include "../pins.h"

#include "RadioSettingsMenu.h"

UnbufferedSerial serial(USBTX, USBRX, 115200);
SerialStream<UnbufferedSerial> pc(serial);

CC1200 radio(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_CS, PIN_RADIO_RST, &pc);
CC1200 dummy(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_DUMMY_CS, PIN_RADIO_DUMMY_RST, &pc);

void configureRFSettings()
{
	askForRadioSettings(pc, radio);

	// Specific to this test suite: enable infinite length mode
	radio.setPacketMode(CC1200::PacketMode::INFINITE_LENGTH, false);
}
const size_t bufferLen = 128; // Size of TX FIFO
char rxBuffer[bufferLen];

const size_t transmissionLen = bufferLen*10;


MovingAverage<float, 100> rssiAverage; // Received Signal Strength Indicator
MovingAverage<float, 100> lqiAverage; // Link Quality Indicator
MovingAverage<float, 10> berAverage; // Byte Error Rate

/**
 * Try to start receiving the data stream.  May fail due to stream not present,
 *
 */
void tryReceiveStream()
{
	pc.printf(">> Trying to key in to data stream\n");

	radio.updateState();
	if(radio.getState() != CC1200::State::IDLE)
	{
		pc.printf("ERROR: Radio not in IDLE state. Actually in %" PRIu8 "\n", static_cast<uint8_t>(radio.getState()));
		return;
	}

	// First, enable RX mode and see if we get any data.
	radio.startRX();
	rssiAverage.clear();
	lqiAverage.clear();

	// Wait for sync detect
	while(!radio.hasReceivedPacket())
	{

	}

	// Figure out where we are in the data stream
	uint8_t lastByte;
	radio.readStream(reinterpret_cast<char *>(&lastByte), 1);

	if(lastByte == 0xAA || lastByte == 0xBB)
	{
		// OK
	}
	else
	{
		pc.printf("ERROR: Received invalid data byte 0x%" PRIx8 ".\n", lastByte);
		berAverage << 0;
		return;
	}

	// Data looks OK, start receiving
	size_t successfulBytes = 0;
	auto timeout = 30ms; // Should take ~2ms to receive 128 bytes, so allow double that
	while(true)
	{
		bool rxSuccessful = radio.readStreamBlocking(rxBuffer, bufferLen, timeout);

		// Check for errors
		if(radio.getState() != CC1200::State::RX)
		{
			pc.printf("ERROR: Radio went to invalid state %" PRIu8 ".\n", static_cast<uint8_t>(radio.getState()));
			break;
		}

		if(!rxSuccessful)
		{
			pc.printf("ERROR: Timeout receiving bytes from transmitter.\n");
			break;
		}

		// check data
		bool dataError = false;
		bool endOfTransmission = false;
		for(size_t index = 0; index < bufferLen; ++index)
		{
			if(lastByte == 0xAA && rxBuffer[index] == 0xBB)
			{
				// OK
				successfulBytes++;
			}
			else if(lastByte == 0xBB && rxBuffer[index] == 0xAA)
			{
				// OK
				successfulBytes++;
			}
			else if(rxBuffer[index] == 0xDD)
			{
				// End of transmission
				endOfTransmission = true;
				break;
			}
			else
			{
				pc.printf("ERROR: Received invalid data byte 0x%" PRIx8 ", last byte was 0x%" PRIx8 ".\n", rxBuffer[index], lastByte);
				dataError = true;
				break;
			}
			lastByte = rxBuffer[index];
		}

		if(endOfTransmission || dataError)
		{
			break;
		}

		// monitor the radio's RSSI
		rssiAverage << radio.getRSSIRegister();
		lqiAverage << radio.getLQIRegister();
	}

	/*pc.printf("Last chunk was:");
	for(size_t byteIndex = 0; byteIndex < bufferLen; ++byteIndex)
	{
		pc.printf(" %" PRIx8, rxBuffer[byteIndex]);
	}
	pc.printf("\n");*/

	berAverage << successfulBytes;

	pc.printf("%zu bytes were successfully received, average RSSI %.02f, average LQI %.02f, average BER %.00f.\n", successfulBytes, rssiAverage.getAvg(), lqiAverage.getAvg(), berAverage.getAvg());
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

	pc.printf(">> Starting receive...\n");

	while(true)
	{
		tryReceiveStream();

		// If attempt fails, we can try again.

		// If all else fails, try hitting it!
		// Ensure RX buffer is clear.
		radio.sendCommand(CC1200::Command::FLUSH_RX);

		// Now go to idle and give the user time to read the message.
		radio.sendCommand(CC1200::Command::IDLE);
		ThisThread::sleep_for(100ms);
	}



}