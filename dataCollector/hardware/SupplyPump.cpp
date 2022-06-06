/** @file SupplyPump.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "SupplyPump.h"
#include "CollectorState.h"

namespace fizz {

extern Logger logger;
extern CollectorState cstate;

/** Constructor for SupplyPump object.
 *
 * @param id is an integer that identifies the pump hardware
 * @param name is the name associated with this pump (one of "referencePump",
 * "reagent1Pump", "reagent2Pump")
 * @param maxRate is the maximum pump rate (if not specified is 1 ml/s).
 * @param maxLevel is the maximum fluid level (if not specified, is 500 ml).
 * @param minLevel is the minimum fluid level for which pump can be turned on
 * (if not specified, is 10 ml).
 */
SupplyPump::SupplyPump(int id, const string& name, double maxRate,
		       double maxLevel, double minLevel)
		       : Pump(id, name, maxRate), changeTime(Util::elapsedTime()) {
	enableFlag = true;
	(this->maxLevel) = maxLevel;
	(this->minLevel) = minLevel;
	fluidLevel = 0;
}

void SupplyPump::initState() {
	Pump::initState();
	fluidLevel = cstate.getSupplyLevel(name);
}

/** Adjust the fluid level, to reflect recent pump activity.
 *  Note: assumes that the caller holds the lock.
 */
void SupplyPump::adjustLevel() {
	double now = Util::elapsedTime();
	if (currentRate != 0) {
		double vol = (currentRate/60.) * (now - changeTime);
		fluidLevel = max(0.0, fluidLevel - vol);
		cstate.setSupplyLevel(name, fluidLevel);
	}
	changeTime = now;
}

/** Get the fluid level for the reservoir.
 *
 * @return the estimated amount of fluid in the reservoir (in milliliters)
 */
double SupplyPump::getLevel(bool quiet) {
	unique_lock<mutex> lck(spMtx);
	adjustLevel();
	if (!quiet)
		logger.trace("SupplyPump (%s)::getLevel returning %d", 
		     	     name.c_str(), (int) fluidLevel);
	return fluidLevel;
}

/** Set the fluid level for this reservoir.
 *
 *  @param level is the specified fluid level (ml)
 */
void SupplyPump::setLevel(double level) {
	unique_lock<mutex> lck(spMtx);
	double mLevel = maxLevel;
	if (level < 0 or level > mLevel)
		logger.warning("Supply::pump::setLevel: specified value "
					   "out-of-range, using limit values");
	enableFlag = true;
	fluidLevel = min(max(0.,level), mLevel);
	cstate.setSupplyLevel(name, fluidLevel);
}

/** Turn on the pump, and update the fluid level.
 *  Do not turn on pump if low on fluid or pump has been disabled.
 *
 *  @param rate is desired flow rate in ml/minute
 */
void SupplyPump::on(double rate) {
	unique_lock<mutex> lck(spMtx);
	adjustLevel();
	if (rate > 0 && (!isEnabled() || fluidLevel < minLevel)) {
		if (!isEnabled())
			logger.error("%s has been disabled; add fluid to re-enable",
						 name.c_str());
		else
			logger.error("Empty supply reservoir for %s", name.c_str());
		Pump::off(); disable();
		return;
	}
	Pump::on(rate);
}

} // ends namespace
