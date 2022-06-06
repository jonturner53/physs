/** @file MixValves.cpp 
 *
 *  @author Jon Turner
 *  @date 2017

 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "MixValves.h"

namespace fizz {

extern Logger logger;

/** Construct a MixValves object.
 */
MixValves::MixValves(const string& name)
					: name(name), mix1Valve(3, "mix1Valve"),
					  midValve(4, "midValve"), mix2Valve(5, "mix2Valve") { }

/** Set the valves to route flow through the mixing coils.
 *
 *  @param useCoil1 if true, flow is routed through the first mixing coil,
 *  otherwise it goes around it.
 *  @param useCoil2 if true, flow is routed through the second mixing coil,
 *  otherwise it goes around it
 */;
void MixValves::select(bool useCoil1, bool useCoil2) {
	unique_lock<mutex> lck(mvMtx);
	mix1Valve.select(useCoil1);
	mix2Valve.select(useCoil2);
	midValve.select(useCoil1 != useCoil2);
}

} // ends namespace
