/** @file Clock.cpp 
 *
 *  @author Jon Turner
 *  @date 2021
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#include "Clock.h"
#include "Arduino.h"
#include "Logger.h"

namespace fizz {

extern Arduino arduino;
extern Logger logger;

string Clock::rtcString = "";
double Clock::rtcTimestamp = 0.;
bool Clock::rtcDisabled = false;

/*
do we need to put rtcDisabled in state file?

if clock stops advancing, we will rediscover it near start of each
cycle and start using system clock

if it starts working, we won't have a valid time value which could be
a nuisance
*/

/** Check realtime clock against stored value and set rtcDisabled flag
 *  if appropriate.
 *  @param s is a realtime clock string from arduino
 *  @return complement of rtcDisabled flag after checking validity of s and
 *  updating flag
 */
bool Clock::rtcCheck(string s) {
	if (rtcDisabled) return false;
	if (s[1] == 'E' || (s.length() != 20 && s.length() != 21)) {
		logger.warning("bad realtime clock string (%s), "
					   "switching to system clock", s.c_str());
		rtcDisabled = true; return false;
	}
    double now = Util::elapsedTime();
	if (now > rtcTimestamp + 5) {
		if (s == rtcString) {
			rtcDisabled = true;
			logger.warning("realtime clock stopped, switching to system clock");
			return false;
		}
		rtcString.assign(s); rtcTimestamp = now;
	}
	return true;
}

/** Get a string representing the current date and time, using system_clock. */
string Clock::sysclockDateTime() {
	time_t tt = system_clock::to_time_t(system_clock::now());
  	struct tm tm = *gmtime(&tt);
	char buf[30];
	strftime(buf, sizeof(buf), "%F %T", &tm);
	return string(buf);
}

/** Get a string representing the current date and time.
 *  If the arduino is equipped with a real-time clock,
 *  the result is based on the value returned by that clock.
 *  If there is no real-time clock, the result is based on the
 *  system_clock.
 */
string Clock::dateTimeString() {
	if (!arduino.isReady()) return sysclockDateTime();
	string s = arduino.query("t"); // format: ss mm hh dd DD MM YY
	if (!rtcCheck(s)) return sysclockDateTime();
	
	return "20" + s.substr(18,2) + "-" + s.substr(15,2) + "-" + 
				  s.substr(12,2) + " " + s.substr(6,2) + ":" +
				  s.substr(3,2) + ":" + s.substr(0,2);
}

/** Set the arduino's real-time clock.
 *  @param s is a string representation of the date and time;
 *  the required format is YYYY-MM-DD hh:mm:ss (e.g. 2021-02-05 17:09:15).
 *  If the provided string does not have exactly 19 characters, the clock
 *  is set from the current time reported by system_clock.
 */
void Clock::setClock(string s) {
	if (s.length() != 19) {
		if (s.length() > 0)
			logger.warning("bad date/time (%s), using system time", s.c_str());
		s = sysclockDateTime();
	}
	arduino.send("T " + s.substr(17,2) + " " + s.substr(14,2) + " " +
						s.substr(11,2) + " 01 " + s.substr(8,2) + " " +
						s.substr(5,2) + " " + s.substr(2,2));
	rtcDisabled = false;
}

} // ends namespace
