/** @file Status.cpp 
 *
 *  @author Jon Turner
 *  @date 2020
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Status.h"
#include "Pump.h"
#include "Config.h"
#include "Operations.h"
#include "Arduino.h"
#include "CollectorState.h"
#include "Clock.h"

using std::this_thread::sleep_for;
using std::chrono::milliseconds;

namespace fizz {

extern Pump samplePump;
extern Config config;
extern CollectorState cstate;
extern Arduino arduino;

/** Constructor for a Status object.  */
Status::Status() {
	vbatParams.offset = 0; vbatParams.scale = 1024. / 16.2;
	tempParams.offset = 0; tempParams.scale = 1024. / 100.;
	upParams.offset = 0; upParams.scale = 1024. / 50;
	downParams.offset = 0; downParams.scale = 1024. / 50;

	vbat = raw(12., vbatParams);
	temp = raw(25., tempParams);
	upPressure = raw(0.1, upParams);
	downPressure = raw(0.1, downParams);
	leakStatus = false;

	maxPressureRecorded = 0.; depthRecorded = 1;
} 

void Status::init() {
	upParams.offset = cstate.getPressureSensor("upstreamOffset");
	upParams.scale = cstate.getPressureSensor("upstreamScale");
	downParams.offset = cstate.getPressureSensor("downstreamOffset");
	downParams.scale = cstate.getPressureSensor("downstreamScale");
}

/** Set pressure parameters.
 *  @param v is an optional argument which (if present) is equal to the
 *  pressure currently being applied to both pressure sensors; in this
 *  case, the current raw pressure values for the two sensors are recorded
 *  for use in calibrating pressure parameters; if v is omitted, the
 *  previously recorded pressure data is used to calibrate the pressure
 *  parameters; note, two datapoints must be recorded before parameters
 *  can be set and the smaller pressure value should be at least 5 psi,
 *  and the larger one should be at least 10 psi greater than the smaller.
 */
bool Status::setPressure(double v) {
	unique_lock<mutex> lck(statusMtx);
	if (v < 0) {
		// set pressure parameters and remove recorded data
		/* simplifying pressure calibration to use 0 offset,
           preserving code to simplify going back if need be
		if (pressureData.size() != 2) return false;
		pdset& pmin = (pressureData[0].cooked < pressureData[1].cooked ?
					   pressureData[0] : pressureData[1]);
		pdset& pmax = (pressureData[0].cooked < pressureData[1].cooked ?
					   pressureData[1] : pressureData[0]);
		if (pmax.cooked - pmin.cooked < 10 || pmin.cooked < 5)
			return false;
		*/
		if (pressureData.size() != 1) return false;
		pdset pmin = { 0.0, 0, 0 };
		pdset& pmax = pressureData[0];
		if (pmax.cooked < 10) return false;

		upParams.scale = (pmax.up - pmin.up)/ (pmax.cooked - pmin.cooked);
		upParams.offset = pmin.up - upParams.scale * pmin.cooked;
		downParams.scale = (pmax.down - pmin.down)/ (pmax.cooked - pmin.cooked);
		downParams.offset = pmin.down - downParams.scale * pmin.cooked;
		cstate.setPressureSensor("upstreamOffset", upParams.offset);
		cstate.setPressureSensor("downstreamOffset", downParams.offset);
		cstate.setPressureSensor("upstreamScale", upParams.scale);
		cstate.setPressureSensor("downstreamScale", downParams.scale);
		pressureData.clear();
	} else {
		// record data
		pressureData.push_back({ v, upPressure, downPressure });
		if (pressureData.size() > 1) // 2)  // part of simplification
			pressureData.erase(pressureData.begin());
	}
	return true;
}

/* Update the status readings.
 * The results are saved in the "cache".
 */
void Status::update() {
	unique_lock<mutex> lck(statusMtx);

	// set dummy values for when no arduino or no control board
	temp = raw(25., tempParams);
	vbat = raw(12., vbatParams);
	double f = samplePump.getCurrentRate() / samplePump.getMaxRate();
	f = max(.05, f);
	upPressure = raw(25 * f, upParams);
	downPressure = raw(5 * f, downParams);
	leakStatus = false;

	time_t tt = system_clock::to_time_t(system_clock::now());
	struct tm tm = *gmtime(&tt);
	char buf[30];
	strftime(buf, sizeof(buf), "%F %T", &tm);
	dateTime = string(buf);

	if (!arduino.isReady()) return;
	string s = arduino.query("s");
	vector<string> words;
	Util::split(s, 12, words);
	if (words.size() < 5 || !arduino.isEquipped()) return;
		// query sent to unequipped arduino to check serial link
	
	vbat = atoi(words[0].c_str());
	temp = atoi(words[1].c_str());
	upPressure = atoi(words[2].c_str());
	downPressure = atoi(words[3].c_str());
	maxPressureRecorded = max(maxPressureRecorded,
							  cook(upPressure, upParams) -
							  cook(downPressure, downParams));
	leakStatus = (words[4] == "1");
	if (words.size() == 12) {
		string rtc = words[5] + " " + words[6]  + " " + words[7] + " " +
					 words[8] + " " + words[9] + " " + words[10] + " " +
					 words[11];
		if (Clock::rtcCheck(rtc))
			dateTime = "20" + words[11] + "-" + words[10] + "-" + words[9] +
					   " "  + words[7]  + ":" + words[6]  + ":" + words[5];
	}
}

} // ends namespace
