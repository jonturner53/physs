/** @file collector.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

/** Data collector process main entry point

Usage: collector 
 */

#include "stdinc.h"

#include "Logger.h"
#include "LogTarget.h"

#include "Arduino.h"
#include "MixValves.h"
#include "PowerControl.h"
#include "LocationSensor.h"
#include "Status.h"
#include "Pump.h"
#include "Spectrometer.h"
#include "SupplyPump.h"
#include "Valve.h"
#include "PowerControl.h"
#include "CommLink.h"
#include "Clock.h"

#include "Config.h"
#include "MaintLog.h"
#include "Console.h"
#include "ConsoleInterp.h"
#include "Operations.h"
#include "CollectorState.h"
#include "Interrupt.h"

namespace fizz {

DataStore dataStore;	// put first so Log2debug can refer to it

// define LogTargets
class Log2debug : public LogTarget {
public:		Log2debug(int level);
	void	logMessage(const string& s, int level) {
		if (level >= logLevel && fs.is_open()) {
			fs << s << std::flush;
			size_t n = s.find("\n");
			if (n == string::npos || n == s.length()-1)
				dataStore.saveDebugRecord(s);
		}
	}
	void	close() { fs.close(); }
private:
	ofstream fs;
} log2debug(Logger::DEBUG);

inline Log2debug::Log2debug(int level) : LogTarget(level) {
	fs.open("debug", ofstream::app);
	if (fs.fail()) {
		cerr << "Log2debug::Log2debug: Cannot open debug" << endl;
	}
}

class Log2stderr : public LogTarget {
public:		Log2stderr(int level) : LogTarget(level) {}
	void	logMessage(const string& s, int level) {
		if (level >= logLevel) cerr << s;
	}
} log2stderr(Logger::ERROR);

Console console;
Logger logger;

Arduino arduino;

// instantiate hardware components
Pump samplePump(1, "samplePump", 5);
SupplyPump referencePump(2, "referencePump",  5, 750, 10);
SupplyPump reagent1Pump(3, "reagent1Pump", 5, 750, 10);
SupplyPump reagent2Pump(4, "reagent2Pump", 5, 750, 10);

Valve portValve(2, "portValve");
Valve filterValve(1, "filterValve");
MixValves mixValves("mixValves");

LocationSensor locationSensor;
Status hwStatus;
Spectrometer spectrometer;
PowerControl powerControl;
CommLink commLink;

// and miscellaneous other things
Config config("config");
MaintLog maintLog("maintLog");
CollectorState cstate("state");
ConsoleInterp consoleInterp;
ScriptInterp scriptInterp;
Interrupt interrupt;

string serialNumber = "0";
string versionNumber = "0.0.9d";
string rootpath = "/usr/local/physs";
string datapath = "/usr/local/physsData";
} // ends namespace

using namespace fizz;
using this_thread::sleep_for;

/** Check for occurrence of a critical failure.
 *  @return true if a critical failure was detected, else false.
 */
bool criticalFailure() {
	static int consecutiveFailures = 0;
	static int failureCount = 0;
	double lowBat = hwStatus.lowBattery();
	double tooHot = hwStatus.tooHot();
	bool leak = hwStatus.leak();

	if (!lowBat && !tooHot && !leak) {
		consecutiveFailures = 0; return false;
	}
	failureCount++;
	if (consecutiveFailures < 10) consecutiveFailures++;
	if (consecutiveFailures < 3) return false;

	if (!config.getIgnoreFailures()) {
		if (lowBat) logger.fatal("Critical failure: low voltage");
		if (tooHot) logger.fatal("Critical failure: excessive heat");
		if (leak) logger.fatal("Critical failure: leak detected");
		return true;
	}

	// generate log messages with increasing intervals in-between
	static double logDelay = 1.;  // time between log messages (1 sec)
	static double lastLogTime = 0.;	 // last time a failure was logged
	double now = Util::elapsedTime();
	if (lastLogTime == 0 || now - lastLogTime > logDelay) {
		if (lowBat)
			logger.error("Critical failure(%d): low voltage", failureCount);
		if (tooHot)
			logger.error("Critical failure(%d): too hot", failureCount);
		if (leak)
			logger.error("Critical failure(%d): leak detected", failureCount);
		lastLogTime = now; logDelay *= 2;
	}
	return false;
}

/** Final processing before exit.
 *  @param critFail is true if we're exiting due to a critical failure
 *  @param exitOnly is a boolean which controls how the program exits;
 *  if true, it simply exits, otherwise it shuts down the beagle bone.
 */
void wrapup(bool critFail, bool exitOnly) {
	consoleInterp.end(); scriptInterp.end();
	consoleInterp.join(); scriptInterp.join();

	powerControl.off();   // turn off power to all the hardware
	logger.info("collector terminating at %s: %s",
		        Clock::dateTimeString().c_str(),
			    (critFail ? "critical failure" : "normal completion")); 
	arduino.log();
	dataStore.close();    // close the raw data file
	log2debug.close();    // final log message
	sleep_for(milliseconds(100));
	arduino.send("S0");   // essential steps are done

	if (!critFail) sleep_for(seconds(15));
		// allow time for messages to propagate to cloud server
        // xfer protocol has 10 second delay between batches
	console.close();
	// commLink.disable();
			// not needed, either we're shutting down in which case
			// it's redundant, or we're not and we want to keep the
			// link to coolcloud operating
	// arduino.finish();	// removed to prevent untimely arduino reset
			// note, this shouldn't be necessary; just protects against
			// undiagnosed intermittent hardware fault in one fizz unit
	if (exitOnly) exit(0);

	// delay shutdown until at least 45 seconds after start
	// to allow admin to login and prevent shutdown
	double now = Util::elapsedTime();
	double delay = 45.;
	if (now < delay)
		sleep_for(milliseconds((int) (1000 * (delay - now))));

	sync(); 
	sleep_for(seconds(30));   // wait for arduino to shutdown BB
							  // no explicit power_off to prevent
						 	  // arduino reset
	exit(0);
	// reboot(RB_POWER_OFF);
}

int main(int argc, char* argv[]) {
	Util::elapsedTime(); // initializes elapsed time clock

	// configure logger
	console.setLevel(Logger::DEBUG);
	log2stderr.setLevel(Logger::DEBUG);
	log2debug.setLevel(Logger::DEBUG);
	logger.addTarget(log2stderr);

	// open console
	if (!console.open("127.0.0.1", 6256)) {
		logger.fatal("cannot open console socket, quitting");
		exit(1);
	}

	sleep_for(seconds(1));
	console.accept();
	logger.addTarget(console);
	logger.info("opened console");

	// read serial number from file at rootpath + "/serialNumber"
    ifstream ifs;
    ifs.open(rootpath + "/serialNumber", ifstream::in);
    if (ifs.fail()) {
        logger.fatal("cannot read serial number file\n");
        exit(1);
    }
	getline(ifs, serialNumber);

	// read config file
	if (!config.read()) {
		logger.fatal("collector: unable to read config file");
		exit(1);
	}

	// adjust log levels to reflect config file values
	console.setLevel(config.getLogLevel("console"));
	log2stderr.setLevel(config.getLogLevel("stderr"));
	log2debug.setLevel(config.getLogLevel("debug"));
	logger.info("read config file");

	// read state file
	if (!cstate.read()) {
		logger.error("collector: unable to read state file");
		exit(1);
	}

	logger.info("read state file");
	// initialize objects that keep data in state file
	samplePump.initState(); referencePump.initState();
	reagent1Pump.initState(); reagent2Pump.initState();
	bool spectrometerStatus = spectrometer.initDevice();
	spectrometer.initState();
	scriptInterp.initState();
	dataStore.initState();
	logger.info("initialized stateful objects");

	logger.addTarget(log2debug);
	logger.border('*');

	// Attempt to connect to arduino and set flags
	int i;
	for (i = 0; i < 10; i++) {
		if (arduino.start()) break;
		arduino.finish();
		sleep_for(seconds(1));
	}
	logger.info("Starting data collector on PHySS %s (ver.%s) at %s",
			     serialNumber.c_str(), versionNumber.c_str(),
				 Clock::dateTimeString().c_str());

	if (!spectrometerStatus) {
		logger.warning("no spectrometer detected: proceeding with "
					   "simulated device");
	}
	if (arduino.isReady()) {
		logger.info(("Arduino present and communicating" +
					 (i == 0 ? "" : " after " + to_string(i) +
								    " failed attempts")).c_str());
		arduino.send(config.getIgnoreFailures() ? "F0" : "F1");
		logger.info("Arduino control board is %s",
				    (arduino.isEquipped() ? "present" : "missing"));
	} else {
		logger.info("No arduino detected, proceeding without it");
	}

	arduino.log();

	powerControl.set(0b00);
	Operations::idleMode(); // idleMode before power on, ensures that pumps
			// come up stopped rather than with random pump speed 
			// from digital pot;

	consoleInterp.begin(); scriptInterp.begin(); // start other threads

	hwStatus.update();		// get status values from arduino
	hwStatus.recordDepth();

	while (true) {
		double t0 = Util::elapsedTime();
		hwStatus.update();
		if (criticalFailure()) {
			// criticalFailure is false when no arduino or no control board
			wrapup(true, false);
		} else if (consoleInterp.zombie()) {
			logger.debug("main: detected consoleInterp termination");
			wrapup(false, true);
		} else if (scriptInterp.zombie()) {
			logger.debug("main: detected scriptInterp termination");
			wrapup(false, !arduino.isReady());
		}
		int interval = (int) (1000 * (Util::elapsedTime() - t0));
		if (interval < 40)
			sleep_for(milliseconds(50 - interval));
	}
}
