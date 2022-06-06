/** \file PowerControl.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#include <mutex>
#include "Util.h"
#include "Logger.h"

namespace fizz {

extern Logger logger;

/** This class provides an api for controlling power to the pumps, valves,
 *  light source and spectrometer.
 */
class PowerControl {
public:
	/** Constructor for power controller. */
		PowerControl() { off(); };

	/** Get the power status as a two bit value. 
	 *  @return the two power bits; bit 1 controls power to the 
	 *  pumps and valves and bit 0 controls power to the light source.
	 */
	int	get() { return status.load(); };

	/** Set the power status.
	 *  @param bits is the new power status.
	 */
	void	set(int bits) {
		unique_lock<mutex> lck(pcMtx);
		string sbits = Util::bits2string(bits,2);
		logger.trace("PowerControl(%s)", sbits.c_str());

		status.store(bits);
		arduino.send("P" + sbits);
	}

	/** Turn on the power to everything */
	void on() { set(0b11); };

	/** Turn off the power to everything */
	void off() { set(0b00); };
private:
	atomic_int status;
	mutex pcMtx;
};

} // ends namespace

#endif
