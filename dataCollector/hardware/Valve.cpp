/** @file Valve.cpp 
 *
 *  @author Jon Turner
 *  @date 2017

 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Valve.h"

namespace fizz {

extern Logger logger;

/** Constructor for a valve object.
 *  @param id is a numeric id for the valve hardware
 *  @param name is a name for the given valve (one of "portValve",
 *  "filterValve", "mix1Valve", "midValve", "mix2Valve").
 */
Valve::Valve(int id, const string& name) : id(id), name(name) { }

} // ends namespace
