/** @file Clock.h
 *
 *  @author Jon Turner
 *  @date 2021
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#ifndef CLOCK_H
#define CLOCK_H

#include "stdinc.h"
#include "Arduino.h"
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

namespace fizz {

/** This class contains miscellaneous utility methods.
 */
class Clock {
public:
	static string dateTimeString();
	static string sysclockDateTime();
	static void	setClock(string="");
	static bool rtcCheck(string);
private:
	static string rtcString;
	static double rtcTimestamp;
	static bool rtcDisabled;
};

} // ends namespace

#endif
