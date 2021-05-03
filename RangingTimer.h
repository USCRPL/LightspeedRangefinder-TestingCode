//
// NOTE: the header for this class was released but the implementation is kept private for now.
//

#ifndef LIGHTSPEEDRANGEFINDER_RANGINGTIMER_H
#define LIGHTSPEEDRANGEFINDER_RANGINGTIMER_H

#include <cstdint>

#include "Utils.h"

/**
 * Timer class for ranging functions.
 *
 * It is similar to Timeline but has a very different purpose: it times ranging
 * operations with near-nanosecond accuracy.
 * It uses a combination of timer capture and timer interrupts to make this work.
 *
 * In contrast, Timeline provides only microsecond-ish accuracy, but is synchronized
 * to an external time source and is able to operate for long periods without rolling over.
 */
class RangingTimer
{

public:

	RangingTimer();

	/**
	 * ISR called when the timer captures a time
	 */
	static void captureInterrupt();

	/**
	 * Configure chip registers for the timer
	 */
	void begin();

	/**
	 * Reset the current time to zero.  Called before a ranging pulse is transmitted.
	 */
	void reset();

	/**
	 * Get the time in nanoseconds since the timer was last reset.
	 * @return
	 */
	chrono::nanoseconds getCurrentTime();

	/**
	 * Get the time in nanoseconds since the second pulse was captured on the sync line
	 * @return
	 */
	chrono::nanoseconds getRxCapturedTime();

	/**
	 * Get the time in nanoseconds since the first pulse was captured on the sync line
	 * @return
	 */
	chrono::nanoseconds getTxCapturedTime();

	/**
	 * Get whether the rangefinder has seen a transmission pulse yet.
	 * @return
	 */
	bool hasSeenTransmission()
	{
		return transmissionTimeCaptured;
	}

	/**
	 * Get whether the rangefinder has received a response from the transmitter yet.
	 * @return
	 */
	bool hasReceivedResponse()
	{
		return receptionTimeCaptured;
	}

};

// global instance
extern RangingTimer rangingTimer;


#endif //LIGHTSPEEDRANGEFINDER_RANGINGTIMER_H
