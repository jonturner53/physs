/** \file Valve.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef VALVE_H
#define VALVE_H

#include <mutex>
#include "Logger.h" 
#include "Util.h" 
#include "Arduino.h"

namespace fizz {

extern Arduino arduino;

/** This class provides an api for controlling a valve.
 */
class Valve {
public:
			Valve(int, const string&);
	void	select(int);
	int		state();
	void	toggle();

	string	getName() { return name; }
private:
	atomic_int branch;	///< the current valve setting
	int		id;			///< numeric id for valve hardware
	string	name;		///< name of this valve

	mutex	vMtx;
};

/** Select one of the valves two branch ports.
 *
 *  @param branch specifies which of the two branch ports (0 or 1)
 *  to connect to the common port.
 */
inline void Valve::select(int nuBranch) {
	unique_lock<mutex> lck(vMtx);
	arduino.send("v" + to_string(id) + to_string(nuBranch));
	branch.store(nuBranch);
}

/** Return the state of the valve.;
 */ 
inline int Valve::state() { return branch.load(); }

/** Toggle the branch selected by the valve.
 */
inline void Valve::toggle() {
	unique_lock<mutex> lck(vMtx);
	int nuBranch = (branch.load()+1) % 2;
	arduino.send("v" + to_string(id) + to_string(nuBranch));
	branch.store(nuBranch);
}

} // ends namespace

#endif
