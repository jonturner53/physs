/** \file SupplyPump.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef SUPPLYPUMP_H
#define SUPPLYPUMP_H

#include <mutex>

#include "Logger.h" 
#include "Exceptions.h"
#include "Pump.h" 

namespace fizz {

/** This class provides an api for controlling a supply pump.
 *  A SupplyPump is combines a pump and a fluid supply reservoir.
 */
class SupplyPump : public Pump {
public:
		SupplyPump(int, const string&, double=1, double=500, double=10);
	void	initState();

	bool	isEnabled() {
				if (fluidLevel < minLevel) enableFlag = false;
				return enableFlag;
			}
	void	disable() { enableFlag = false; }
	double	getLevel(bool=false);
	void	setLevel(double);
	void	on(double);
	void	off() { on(0); }

	double available() {
		return max(0.0, getLevel() - getMinLevel());
	};
	double	getMaxLevel() {
		unique_lock<mutex> lck(spMtx);
		return maxLevel;
	};
	void	setMaxLevel(double level) {
		unique_lock<mutex> lck(spMtx);
		maxLevel = level;
	};
	double	getMinLevel() {
		unique_lock<mutex> lck(spMtx);
		return minLevel;
	};
	void	setMinLevel(double level) {
		unique_lock<mutex> lck(spMtx);
		minLevel = level;
	};

private:
	bool	enableFlag;
	double	maxLevel;
	double	minLevel;
	double	fluidLevel;
	double	changeTime;

	void	adjustLevel();

	mutex	spMtx;
};

} // ends namespace

#endif
