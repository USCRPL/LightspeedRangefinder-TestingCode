//
// Created by jamie on 3/17/2020.
//

#include <mbed.h>
#include <SerialStream.h>

#include <CC1200.h>
#include <cinttypes>

#include "../QuickStats.h"
#include "../RangingTimer.h"
#include "../pins.h"

#include "RadioSettingsMenu.h"

BufferedSerial serial(USBTX, USBRX, 115200);
SerialStream<BufferedSerial> pc(serial);

CC1200 txRadio(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_CS, PIN_RADIO_RST, &pc);
CC1200 rxRadio(PIN_RADIO_SPI_MOSI, PIN_RADIO_SPI_MISO, PIN_RADIO_SPI_SCLK, PIN_RADIO_DUMMY_CS, PIN_RADIO_DUMMY_RST, &pc);

//CC1200 txRadio(PIN_RSPI_MOSI, PIN_RSPI_MISO, PIN_RSPI_SCLK, PIN_RX_CS, PIN_RX_RST, &pc);
//CC1200 rxRadio(PIN_RSPI_MOSI, PIN_RSPI_MISO, PIN_RSPI_SCLK, PIN_TX_CS, PIN_TX_RST, &pc);

// These have an effect on jitter:
// - Symbol rate (seems to be about linear with jitter)
// - Intermediate frequency (increases jitter if below a certain level, but no effect once above that point))

// These do NOT have an effect on jitter:
// - Sync word length
// - FSK vs GFSK modulation
// - RX filter bandwidth
// - Preamble length (maybe like a 5% jitter decrease?  Or maybe it was just random)

// TODO for final tests:
// - Configure FS calibration to happen constantly
// - Increase speed to maximum possible

// Other things to try:
// - Configure PA ramp rate
// - Increase or decrease length of preamble
// - Configure AGC

void checkExistance()
{
	pc.printf("Checking TX radio.....\n");
	bool txSuccess = txRadio.begin();
	pc.printf("TX radio initialized: %s\n", txSuccess ? "true" : "false");

	pc.printf("Checking RX radio.....\n");
	bool rxSuccess = rxRadio.begin();
	pc.printf("RX radio initialized: %s\n", rxSuccess ? "true" : "false");
}

const size_t numTrials = 300;
std::array<int64_t, numTrials> roundtripTimes; //time in ns for each trial

void checkSignalTransmit()
{
	pc.printf("Initializing CC1200s.....\n");
	txRadio.begin();
	rxRadio.begin();
	rangingTimer.begin();

	pc.printf("Configuring RF settings.....\n");

	askForRadioSettings(pc, rxRadio);
	askForRadioSettings(pc, txRadio);

	// The RX radio will act as the "ground station" radio.  It will transmit a message,
	// then wait for a response.  The processor, using the radio's sync outputs, records the timestamp
	// of both events and turns that into the ranging time.

	// The TX radio will act as the "transponder" radio.  It will initially in receive mode, with a packet queued.
	// Then, when it receives a message, it will automatically switch to TX mode and send its current buffer.

	// rename for less confusion
	CC1200 & groundStation = std::ref(rxRadio);
	CC1200 & transponder = std::ref(txRadio);

	// configure on-transmit actions
	transponder.setOnTransmitState(CC1200::State::RX);
	transponder.setOnReceiveState(CC1200::State::TX, CC1200::State::RX);

	groundStation.setOnTransmitState(CC1200::State::RX);
	groundStation.setOnReceiveState(CC1200::State::FAST_ON, CC1200::State::RX);

	groundStation.configureGPIO(0, CC1200::GPIOMode::PKT_SYNC_RXTX);
	groundStation.configureGPIO(2, CC1200::GPIOMode::PKT_SYNC_RXTX);

	pc.printf("Starting transmission.....\n");

	const char groundStationMessage[] = "Hello world!";
	const char transponderMessage[] = "Hi back";
	char packetBuffer[std::max(sizeof(groundStationMessage), sizeof(transponderMessage)) + 1];

	// make sure there's a null terminator even if data is corrupted
	packetBuffer[sizeof(packetBuffer) - 1] = '\0';

	size_t packetsReceived = 0;

	// data for calculating jitter

	// TEMP: manually calibrate FS
	groundStation.sendCommand(CC1200::Command::CAL_FREQ_SYNTH);
	transponder.sendCommand(CC1200::Command::CAL_FREQ_SYNTH);
	ThisThread::sleep_for(1ms);
	while(groundStation.getState() == CC1200::State::CALIBRATE || transponder.getState() == CC1200::State::CALIBRATE)
	{}
	transponder.startRX();

	for(size_t trialIndex = 0; trialIndex < numTrials; ++trialIndex)
	{
		// initial conditions:
		// - ground station is in TX mode with no data (so it isn't transmitting)
		// - transponder is in RX mode with a packet queued for as soon as it goes into TX mode
		transponder.enqueuePacket(transponderMessage, sizeof(transponderMessage));

		wait_ns(rand() % 5000);

		//pc.printf("Ground station radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, RX FIFO len = 0x%u\n",
		//		  static_cast<uint8_t>(groundStation.getState()), groundStation.getTXFIFOLen(), groundStation.getRXFIFOLen());
		pc.printf("\n---------------------------------\n");

		//pc.printf("Before tx: Ground station radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, RX FIFO len = 0x%u\n",
		//		  static_cast<uint8_t>(groundStation.getState()), groundStation.getTXFIFOLen(), groundStation.getRXFIFOLen());

		pc.printf("<<SENDING TO TRANSPONDER: %s\n", groundStationMessage);
		rangingTimer.reset(); // reset timer ensuring rollover won't happen
		groundStation.startTX();

		// Give time for radio to get into TX mode
		while(groundStation.getState() != CC1200::State::TX) {
			groundStation.updateState();
		}

		groundStation.enqueuePacket(groundStationMessage, sizeof(groundStationMessage));

		// wait for message and response to go through
		Timer responseTimer;
		responseTimer.start();
		while(!groundStation.hasReceivedPacket())
		{
			if(responseTimer.elapsed_time() > 100ms)
			{
				pc.printf("Timeout waiting for response\n");
				break;
			}
		}

		roundtripTimes[trialIndex] = (rangingTimer.getRxCapturedTime() - rangingTimer.getTxCapturedTime()).count();


		// get the results
		pc.printf("Ground station radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, RX FIFO len = 0x%u\n",
				  static_cast<uint8_t>(groundStation.getState()), groundStation.getTXFIFOLen(), groundStation.getRXFIFOLen());
		if(groundStation.hasReceivedPacket())
		{
			++packetsReceived;
			groundStation.receivePacket(packetBuffer, sizeof(packetBuffer));
			pc.printf(">>RECEIVED: %s\n", packetBuffer);
		}
		else
		{
			pc.printf(">>No packet received!\n");
		}

		pc.printf("Transponder radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, RX FIFO len = 0x%u\n",
				static_cast<uint8_t>(transponder.getState()), transponder.getTXFIFOLen(), transponder.getRXFIFOLen());

		if(transponder.hasReceivedPacket())
		{
			transponder.receivePacket(packetBuffer, sizeof(packetBuffer));
			pc.printf(">>RECEIVED: %s\n", packetBuffer);
		}
		else
		{
			pc.printf(">>No packet received!\n");
		}

		pc.printf("Elapsed time was %" PRIi64 " (TX time = %" PRIi64 ", RX time = %" PRIi64 ")\n", roundtripTimes[trialIndex], rangingTimer.getTxCapturedTime().count(), rangingTimer.getRxCapturedTime().count());
	}

	QuickStats<int64_t, numTrials> stats(roundtripTimes);

	pc.printf("Packets received %zu (%.00f%%)\n", packetsReceived, (packetsReceived / static_cast<float>(numTrials)) * 100.0f);
	pc.printf("Average Round-Trip Time: %.00f ns\n", stats.average);
	pc.printf("Jitter: +-%" PRIu64 " ns\n", (stats.maxVal - stats.minVal) / 2);
	pc.printf("Standard Deviation: %.00f ns\n", stats.stdDeviation);

}

// This test checks the ranging timer RX radio sync capture input.
// It starts the ranging timer, waits a certain amount of us, then
// manually toggles the chip GPIO high.
void checkRXTimerCapture()
{
#define RADIO txRadio
#define GPIO 2
#define OFFSET 32us // measured offset of the timing tests

	RADIO.begin();
	RADIO.configureGPIO(GPIO, CC1200::GPIOMode::HW0); // GPIO 2 is connected to RX timer capture

	rangingTimer.begin();

	// times to delay
	const uint32_t timesus[] = {1, 10, 50, 100, 170, 6000, 60000};
	const size_t numTests = sizeof(timesus) / sizeof(uint32_t);

	for(size_t testIndex = 0; testIndex < numTests; ++testIndex)
	{
		ThisThread::sleep_for(500ms);

		pc.printf("\n---------------------------------\n");
		pc.printf("Starting test %zu, which delays for %" PRIu32 "us\n", testIndex, timesus[testIndex]);

		// start timer and wait the appropriate amount of time
		rangingTimer.reset();
		//pc.printf("After reset, interrupt triggered: %d\n", rangingTimer.hasSeenTransmission());
		//pc.printf("After reset timer measured: %" PRIu64 " us\n",
		//		  chrono::duration_cast<chrono::microseconds>(rangingTimer.getTxCapturedTime()).count());

		wait_us(timesus[testIndex]);

		// time has elapsed, toggle GPIO on for a moment to simulate receiving a packet
		RADIO.configureGPIO(GPIO, CC1200::GPIOMode::HW0, true);
		auto onTime = rangingTimer.getCurrentTime();
		ThisThread::sleep_for(500ms);
		wait_us(10);
		RADIO.configureGPIO(GPIO, CC1200::GPIOMode::HW0, false);

		// delay for a while longer before reading the timer so it's obvious if the captured value is being updated all the time
		ThisThread::sleep_for(500ms);

		pc.printf("Interrupt triggered: %d\n", rangingTimer.hasSeenTransmission());
		pc.printf("Timer measured: %" PRIu64 " us, expected approximately: %" PRIu64 " us\n",
			chrono::duration_cast<chrono::microseconds>(rangingTimer.getTxCapturedTime() - OFFSET).count(),
			chrono::duration_cast<chrono::microseconds>(onTime - OFFSET).count());
	}
}

int main()
{
	pc.printf("\nHamster Radio Test Suite:\n");

	while(1){
		int test=-1;
		//MENU. ADD AN OPTION FOR EACH TEST.
		pc.printf("Select a test: \n");
		pc.printf("1.  Exit Test Suite\n");
		pc.printf("2.  Check Existance\n");
		pc.printf("3.  Check Transmitting Signal\n");
		pc.printf("4.  Check RX timer capture\n");

		pc.scanf("%d", &test);
		printf("Running test %d:\n\n", test);
		//SWITCH. ADD A CASE FOR EACH TEST.
		switch(test) {
			case 1:         pc.printf("Exiting test suite.\n");    return 0;
			case 2:         checkExistance();              break;
			case 3:         checkSignalTransmit();              break;
			case 4:         checkRXTimerCapture();              break;
			default:        pc.printf("Invalid test number. Please run again.\n"); continue;
		}
		pc.printf("done.\r\n");
	}

	return 0;

}
