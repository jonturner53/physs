/** \file MixValves.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef MIXVALVES_H
#define MIXVALVES_H

#include <mutex>
#include "Logger.h" 
#include "Valve.h" 

namespace fizz {

/** This class provides an api for controlling the mixing valves.
 */
class MixValves {
public:
			MixValves(const string&);
	void	select(bool, bool);
	int	state() { return (mix1Valve.state() << 1) | mix2Valve.state(); }

	string	name;
private:
	Valve	mix1Valve;
	Valve	midValve;
	Valve	mix2Valve;

	mutex	mvMtx;
};

} // ends namespace

#endif
