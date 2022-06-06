/** \file Pump.cpp
 *  @author Jon Turner
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Pump.h"
#include "CollectorState.h"
#include "Arduino.h"

namespace fizz {

extern Logger logger;
extern CollectorState cstate;

extern Arduino arduino;

/** Constructor for Pump object.
 *  @param id is an integer used to identify the pump hardware
 *  @param name is a user-friendly name for the pump object
 *  @param maxRate is the maximum pump rate.
 *  if unspecified, is assigned value of 1.
 */
Pump::Pump(int id, const string& name, double maxRate) : 
			maxRate(maxRate), name(name), id(id) {
	currentRate = 0;
}

/** Initialize pump state variables.
 *  Should only be called from main thread, before other threads start running.
 */
void Pump::initState() {
	maxRate = cstate.getMaxRate(name);
}

/** Set maximum pump rate.
 *
 *  @param rate is the new maximum pump rate (in ml/m)
 *  note: when you set the max rate, you are specifying the
 *  flow rate that corresponds to the maximum pump speed; you are
 *  not specifying a limit on the flow rate that is smaller than
 *  the maximum pump speed; should only be used when calibrating pumps
 */
void Pump::setMaxRate(double rate) {
	unique_lock<mutex> lck(puMtx);
	if (rate >= 0) {
		maxRate = rate;
	} else {
		logger.error("max rate must be non-negative");
	}
	cstate.setMaxRate(name, rate);
}

/** Turn the pump on at a specified rate.
 *
 *  @param rate specifies the rate at which pump is to be run (in ml/m).
 */
void Pump::on(double rate) {
	unique_lock<mutex> lck(puMtx);
	if (rate < -maxRate) {
		logger.warning("pump.on(): excessive pump rate changed to %.3f",
						-maxRate);
		rate = -maxRate;
	} else if (rate > maxRate) {
		logger.warning("pump.on(): excessive pump rate changed to %.3f",
					   maxRate);
		rate = maxRate;
	}
	
	if (rate != 0) {
		logger.debug("%s on at rate %.3f", name.c_str(), rate);
	} else if (currentRate != 0) {
		logger.debug("%s off", name.c_str());
	}
	currentRate = rate;

	if (!arduino.isReady()) return;

	int speed; unsigned fspeed;
	if (id <= 2) {
		speed = (int) (2040.0 * (rate/maxRate));
		fspeed = (unsigned) (speed + 2048); // rotate range
		arduino.send("p" + to_string(id) + to_string(fspeed));
	} else {
		fspeed = (unsigned) (4090.0 * (rate/maxRate));
		arduino.send("p" + to_string(id) + to_string(fspeed));
	}
}

} // ends namespace
