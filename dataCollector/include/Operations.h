/** \file Operations.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "stdinc.h" 
#include "Util.h"
#include "Exceptions.h"
#include "Logger.h"

using namespace std;

namespace fizz {

/** This static class contains encapsulates a number of common operations.
 */
class Operations {
public:	
	static void idleMode();
	static void referenceSample(double, double, double);
	static bool optimizeIntegrationTime(double, double, double);
//	static bool adjustIntegrationTime(double&);
	static void computePumpRates(double&, double, double,
				     double&, double&, double&);
	static void unfilteredSample(double, double, double, double);
	static void filteredSample(double, double, double, double);
	static void filteredSampleAdaptive(double, double, double);
	static bool adjustRates(double&, double, double, double,
				double, double, double&);
	static void flushFilter();
	static void purgeBubbles();
	static void flush();
	static string optimizeConcentration(
						double, double, double, double, double);
};

extern Logger logger;

} // ends namespace

#endif
