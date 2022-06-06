/** @file ConsoleInterp.cpp 
 *
 *  @author Jon Turner
 *  @date 2017

 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Util.h"
#include "SocketAddress.h"
#include "Socket.h"
#include "StreamSocket.h"

#include "Pump.h"
#include "SupplyPump.h"
#include "Valve.h"
#include "MixValves.h"
#include "LocationSensor.h"
#include "Status.h"
#include "Spectrometer.h"
#include "CommLink.h"
#include "PowerControl.h"
#include "Clock.h"

#include "Config.h"
#include "MaintLog.h"
#include "CollectorState.h"
#include "Console.h"
#include "ConsoleInterp.h"
#include "DataStore.h"
#include "ScriptInterp.h"
#include "Operations.h"
#include "Interrupt.h"

namespace fizz {

extern Logger logger;
extern LogTarget log2stderr;
extern LogTarget log2debug;

extern Pump samplePump;
extern SupplyPump referencePump;
extern SupplyPump reagent1Pump;
extern SupplyPump reagent2Pump;
extern Valve portValve;
extern Valve filterValve;
extern MixValves mixValves;
extern Spectrometer spectrometer;
extern LocationSensor locationSensor;
extern Status hwStatus;
extern PowerControl powerControl;
extern CommLink commLink;

extern Config config;
extern MaintLog maintLog;
extern CollectorState cstate;
extern Console console;
extern ScriptInterp scriptInterp;
extern DataStore dataStore;
extern Interrupt interrupt;

extern string serialNumber;
extern string versionNumber;
extern Arduino arduino;

using this_thread::sleep_for;

/** Constructor for ConsoleInterp object.
 *  to the script interpreter.
 */
ConsoleInterp::ConsoleInterp() {
	scriptPath = "script";
}

void consoleIntHandler() {}

void ConsoleInterp::startThread(ConsoleInterp& ci) { ci.run(); }

void ConsoleInterp::begin() {
	unique_lock<mutex> lck(mtx);
	logger.details("ConsoleInterp: starting thread");
	myThread = thread(&ConsoleInterp::startThread, ref(*this));
	thread_id = myThread.get_id();
	interrupt.register_client(thread_id, "console interpreter",
							  &consoleIntHandler);
}

void ConsoleInterp::end() {
	unique_lock<mutex> lck(mtx);
	logger.details("ConsoleInterp: terminating thread");
	quitFlag = true; interrupt.request(thread_id, true);
}

void ConsoleInterp::join() {
	if (myThread.joinable()) myThread.join();
}


/** This method opens the script file and reads the script.
 *  @return true if the script was read and passed all
 *  syntax checks, else false
 */
bool ConsoleInterp::readScript() {
	int lineNum = scriptInterp.readScript(scriptPath);
	if (lineNum < 0) {
		logger.warning("unable to open script file: %s",
				scriptPath.c_str());
		return false;
	} else if (lineNum != 0) {
		logger.error("syntax error in script file, line %d", lineNum);
		return false;
	}
	return true;
}

/** Send reply to console and log it.
 */
void ConsoleInterp::reply(const string& s) {
	console.reply(s);
	logger.debug(("sending reply: " + s).c_str());
}

/** This method implements the main loop between
 *  the PSS1 process and a separate console program.
 *  The console may connect/disconnect multiple times,
 *  but we only support one connection at a time.
 *
 *  This method processes command lines received from a remote
 *  console. It parses the command line, checks for valid
 *  arguments and carries out the command. It returns a string,
 *  to be returned to the remote user-interface (may an error
 *  message or requested information).
 */
void ConsoleInterp::run() {
	logger.debug("ConsoleInterp: starting");

	int autoRun = config.getAutoRun();
	autoRunFlag = (autoRun >= 0);
	double deadline = (autoRunFlag ? 
		Util::elapsedTime() + 60. * autoRun : 1.0e12);

	hwStatus.recordDepth();
	locationSensor.recordLocation();

	while (!quitFlag) {
		if (zombieFlag) {
			sleep_for(milliseconds(50)); continue;
		}

		if (autoRunFlag && Util::elapsedTime() > deadline) {
			if (!readScript()) {
				logger.error("script error, try again");
			} else if (!config.read()) {
				logger.error("error in config file");
			} else {
    			log2stderr.setLevel(config.getLogLevel("stderr"));
    			log2debug.setLevel(config.getLogLevel("debug"));
				scriptInterp.resume();
				autoRunFlag = false;
			}
		}

		if (!console.isConnected()) {
			logger.trace("console disconnected, attempting to reconnect");
			int status = console.accept();
			if (status == -1) {
				sleep_for(milliseconds(50)); continue;
			} else if (status < 0) {
				cerr << "ConsoleInterp: error during accept\n";
				logger.trace("consoleInterp: socket error "
						 	 "while accepting connection");
				sleep_for(milliseconds(50)); continue;
			}
		}

		// connected, so read line from the console
		string line;
		int status = console.readline(line);
		if (status == 0) {
			logger.error("ConsoleInterp: lost connection to peer, "
						 "will try to reconnect");
			continue;
		} else if (status == -1) {
		 	sleep_for(milliseconds(50)); continue;
				// no complete line available yet
		} else if (status < 0) {
			cerr << "ConsoleInterp: error during readline\n";
			logger.error("ConsoleInterp: error while reading from socket");
			break;
		}

		// and process line
		if (line.length() == 0) continue;
		vector<string> words;
		Util::split(line, 5, words);
		if (words.size() == 0) continue;
		if (words[0] == "quit") {
			logger.details("received quit command");
			reply("quitting on command");
			zombieFlag = true;
			continue;
		} else if (words[0] == "close") {
			logger.details("received close command");
			reply("closing console connection");
			console.close();
		} else {
			if (words[0] != "snapshot") {
				logger.debug("received command: %s", line.c_str());
			}
			if (calibrationInProgress && words[0] != "snapshot" &&
				words[0].find("Pump") == string::npos) {
				reply("command not allowed while calibration in progress");
			} else {
				try {
					doCommand(words);
				} catch(exception& e) {
					reply("ConsoleInterp::run(): caught "
						   + string(e.what()) + ", try again");
					continue;
				}
			}
		}
	}
}

/** This method carries out a single console command.
 *  @param words is a non-empty vector of words that represents a command line.
 *
 *  The available commands are listed below.
 *
 *  start	   	start execution of sample collection script
 *  stop		suspend execution of sample collection script
 *  resume	  	resume sample collection (at start of current 
 *			  	sample cycle)
 *  quit			closedown gracefully and exit
 *  
 *  samplePump [ ((on [rate])|off) | calibrate ]
 *  referencePump [ ((on [rate])|off) | calibrate ] 
 *  reagent1Pump [ ((on [rate])|off) | calibrate ]
 *  reagent2Pump [ ((on [rate])|off) | calibrate ]
 *
 *  referenceSupply [volume]
 *  reagent1Supply [volume]
 *  reagent2Supply [volume]
 *
 *  filterValve [0|1]	  	get/set valve state
 *  mixValves [bb]
 *  portValve [0|1]
 *
 *  lights [bbb]			turn lights on/off, set shutter
 *  spectrometer b			turn spectrometer on/off
 *  integrationTime [time] 	get/set integration time
 *  spectrum [bbb]			read spectrum
 *
 *  power (on|off|bb)  		turn power on/off
 *  pressure 		 		read the pressure sensors
 *  deploymentLabel			get deployment label
 *  serialNumber 	   		get serial number
 *  versionNumber 	  		get software version number
 *  logLevel levelName		get/set log level
 *  cycleNumber 			get cycleNumber
 *  reload file				re-read script or config
 */
void ConsoleInterp::doCommand(vector<string>& words) {
	if (words[0].find("Pump") != string::npos) {
		pumpControl(words);
	} else if (words[0].find("Supply") != string::npos) {
		fluidSupplyControl(words);
	} else if (words[0].find("Valve") != string::npos) {
		valveControl(words);
	} else if (words[0] == "stop") {
		if (scriptInterp.samplingEnabled()) {
			reply("stopping script interpreter");
			scriptInterp.stop();
		} else {
			reply("this operation only allowed when sampling is enabled");
		}
	} else if (words[0] == "resume") {
		if (!scriptInterp.samplingEnabled()) {
			if (!readScript()) {
				reply("script error, try again");
			} else if (!config.read()) {
				reply("error in config file, try again");
			} else {
    			log2stderr.setLevel(config.getLogLevel("stderr"));
    			log2debug.setLevel(config.getLogLevel("debug"));
				reply("resuming script interpreter");
				scriptInterp.resume();
				autoRunFlag = false;
			}
		} else {
			reply("this operation only allowed when sampling is disabled");
		}
	} else if (words[0] == "start") {
		if (!scriptInterp.samplingEnabled()) {
			if (!readScript()) {
				reply("script error, try again");
			} else if (!config.read()) {
				reply("error in config file, try again");
			} else if (!maintLog.read()) {
				reply("cannot read maintenance log");
			} else {
    			log2stderr.setLevel(config.getLogLevel("stderr"));
    			log2debug.setLevel(config.getLogLevel("debug"));
				reply("starting script interpreter");
				scriptInterp.start();
				autoRunFlag = false;
			}
		} else {
			reply("this operation only allowed when sampling is disabled");
		}
	} else if (words[0] == "lights") {
		if (powerControl.get() != 0b11) powerControl.on();
		if (words.size() == 1) {
			reply("light status is " +
				Util::bits2string(spectrometer.getLights(), 3));
			return;
		} else if (scriptInterp.samplingEnabled()) {
			reply("cannot perform this operation while "
				  "automated sampling is enabled");
			return;
		}
		if (words[1].length() < 3) {
			reply("usage: lights [ bbb ]");
			return;
		}
		int lconfig = Util::string2bits(words[1]);
		spectrometer.setLights(lconfig);
		reply("light status is now " + words[1]);
	} else if (words[0] == "integrationTime") {
		char buf[20];
		if (words.size() == 1) {
			snprintf(buf, sizeof(buf), "%.2f",
					 spectrometer.getIntTime());
			reply("integration time is " + string(buf));
			return;
		}
		double integTime = strtod(words[1].c_str(),0);
		snprintf(buf, sizeof(buf), "%.2f", integTime);
		reply("setting integration time to " + string(buf)) ;
		spectrometer.setIntTime(integTime);
	} else if (words[0] == "spectrum") {
		if (powerControl.get() != 0b11) powerControl.on();
		int lconfig = 0;
		if (words.size() == 1) {
			lconfig = spectrometer.getLights();
		} else if (words[1].length() < 3) {
			reply("usage: spectrum [ bbb ]");
			return;
		} else {
			lconfig = Util::string2bits(words[1]);
		}
		spectrometer.getSpectrum(lconfig);
		unsigned int ssiz = spectrometer.spectrum.size();
		const int binSize = 147;	// mostly equal-size bins of about 50 nm
		const unsigned int numBins = 1 + ssiz/binSize;
		double bins[numBins];
		for (unsigned int i = 0; i < numBins; i++) bins[i] = 0.0;
		for (unsigned int i = 0; i < ssiz; i++) {
			bins[i/binSize] += spectrometer.spectrum[i];
		}
		interrupt.check();
		string s; char buf[100];
		for (unsigned int i = 0; i < numBins; i++) {
			if (i > 0) s += ", ";
			snprintf(buf, sizeof(buf), "%.0f", bins[i]/binSize);
			s += buf;
		}
		reply("[" + s + "]");
	} else if (words[0] == "power") { 
		if (words.size() == 1) {
			reply("power is " + Util::bits2string(powerControl.get(), 2));
		} else if (words[1] == "on") {
			powerControl.on();
		} else if (words[1] == "off") {
			powerControl.off();
		} else if (words[1].size() == 2) {
			reply("setting power to " + words[1]);
			powerControl.set(Util::string2bits(words[1]));
		} else {
			reply("usage: power (on|off|bb)");
		}
	} else if (words[0] == "batteryVoltage") {
		double bv = hwStatus.voltage();
		char buf[100];
		snprintf(buf, 100, "battery voltage is %.2f volts", bv);
		reply(buf);
	} else if (words[0] == "disableCommLink") {
		reply("disabling communications link");
		sleep_for(seconds(2));
		commLink.disable();
	} else if (words[0] == "pressure") { 
		char buf[100];
		if (words.size() == 1) {
			snprintf(buf, sizeof(buf),
					 "pressure: %.2f %.2f psi (%d, %d)",
				 	 hwStatus.upstreamPressure(),
				 	 hwStatus.downstreamPressure(),
				 	 hwStatus.upstreamRawPressure(),
				 	 hwStatus.downstreamRawPressure());
			reply(buf);
		} else if (words[1] == "set") {
			if (words.size() == 2) {
				if (hwStatus.setPressure())
					reply("updated pressure parameters");
				else
					reply("not able to update pressure parameters");
			} else {
				double pressure = strtod(words[2].c_str(),0);
				hwStatus.setPressure(pressure);
				snprintf(buf, sizeof(buf),
						 "recorded pressure data: %.2f %d %d",
						 pressure, hwStatus.upstreamRawPressure(),
								   hwStatus.downstreamRawPressure());
				reply(buf);
			}
		} else {
			reply("usage: pressure [ set [ pvalue ] ]");
		}
	} else if (words[0] == "depth") { 
		char buf[100];
		snprintf(buf, 100, "depth is %.2f meters", hwStatus.depth());
		reply(buf);
		return;
	} else if (words[0] == "gps" || words[0] == "location") { 
		reply("gps coordinates are " +
				locationSensor.getRecordedLocation().toString());
	} else if (words[0] == "logLevel") { 
		if (words.size() == 1) {
			reply("log level is " + logger.logLevel2string(console.getLevel()));
		} else {
			reply("setting log level to " + words[1]);
			console.setLevel(logger.string2logLevel(words[1]));
		}
	} else if (words[0] == "snapshot") { 
		snapshot(words);
	} else if (words[0] == "cycleNumber") {
		reply("cycleNumber is " + to_string(scriptInterp.getCycleNumber()));
	} else if (words[0] == "optimizeConcentration") { 
		double filtVol = 10 * .35;  // filter volume is .35 ml
		double filtRate = 1;	// 3.5 minutes for filtered sample
		double unfVol = .05;	// tubing volume is .2-.4 ml
		double unfTot = 1;		// waveguide volume is .067 ml
		double unfRate = 1;		// 3 seconds per squirt
			// with default parameters, expect waveguide to get
			// to max concentration by 5-9 samples and stay high
			// for another 7 samples
		if (words.size() == 6) {
			filtVol = strtod(words[1].c_str(), 0);
			filtRate = strtod(words[2].c_str(), 0);
			unfVol = strtod(words[3].c_str(), 0);
			unfRate = strtod(words[4].c_str(), 0);
			unfTot = strtod(words[5].c_str(), 0);
		}
		char buf[100];
		snprintf(buf, sizeof(buf), "%4.2f %4.2f %5.3f %4.2f %4.2f",
				 filtVol, filtRate, unfVol, unfTot, unfRate);
		reply("optimizing concentration " + string(buf) + " be patient");
		string s = Operations::optimizeConcentration(
						filtVol, filtRate, unfVol, unfTot, unfRate);
		console.logMessage(s + "\n");
	} else if (words[0] == "check4faults") {
		if (!arduino.isReady()) {
			reply("arduino not detected");
		} else if (!arduino.isEquipped()) {
			reply("arduino running without hardware");
		} else if (words.size() == 2 && words[1] == "0") {
			arduino.send("F0");
			reply("turning off fault checking");
		} else if (words.size() == 1 || words[1] == "1") {
			arduino.send("F1");
			reply("turning on fault checking");
		}
	} else if (words[0] == "reload") { 
		if (scriptInterp.samplingEnabled()) {
			logger.error("ConsoleInterp: this operation not "
					 	 "allowed while sampling in progress\n");
		} else if (words[1] == "script") {
			if (!readScript()) {
				logger.error("ConsoleInterp:: cannot reload script");
				reply("Error in script file, try again");
			} else {
				dataStore.saveScriptRecord();
				logger.info("ConsoleInterp:: reloaded and saved script");
				reply("Reloaded script file and saved to data file");
			}
		} else if (words[1] == "config") {
			if (!config.read()) {
				logger.error("ConsoleInterp:: cannot reload config");
				reply("Error in config file, try again");
			} else {
    			log2stderr.setLevel(config.getLogLevel("stderr"));
    			log2debug.setLevel(config.getLogLevel("debug"));
				if (arduino.isEquipped())
					arduino.send(config.getIgnoreFailures() ? "F0" : "F1");
				dataStore.saveConfigRecord();
				logger.info("ConsoleInterp:: reloaded and saved config");
				reply("Reloaded config file and saved to data file");
			}
		} else if (words[1] == "maintLog") {
			if (!maintLog.read()) {
				logger.error("ConsoleInterp:: cannot reload maintLog");
				reply("Cannot reload maintenance log file");
			} else {
				logger.info("ConsoleInterp:: reloaded maintenance file");
				reply("Reloaded maintenance file");
			}
		} else {
			logger.error("ConsoleInterp:: invalid reload target");
			reply("invalid reload target " + words[1]);
		}
	} else if (words[0] == "setClock") {
		string dateTime;
		if (words.size() == 3)
			dateTime = words[1] + " " + words[2];
		Clock::setClock(dateTime);
		reply("time set to " + Clock::dateTimeString());
	} else {
		reply("unrecognized command: " + words[0]);
	}
}

/** Generate a snapshot of the system"s state.
 *
 *  Creates a dictionary containing the state, and sends
 *  a json repreresentation to console.
 */
void ConsoleInterp::snapshot(vector<string>& words) {
	char buf[2000];

	snprintf(buf, sizeof(buf), "snapshot reply {"
		"\"dateTime\": \"%s\", \"cycleNumber\": %ld, "
		"\"currentLine\": %d, "
		"\"samplingEnabled\": %d, \"hardwareConfig\": \"%s\", "
		"\"samplePump\": %.2f, "
		"\"referencePump\": %.2f, "
		"\"reagent1Pump\": %.2f, "
		"\"reagent2Pump\": %.2f, "
		"\"referenceSupply\": %d, \"reagent1Supply\": %d, "
		"\"reagent2Supply\": %d, "
		"\"filterValve\": %d, \"mixValves\": \"%s\", \"portValve\": %d, "
		"\"lights\": \"%s\", \"spectrometer\": %d, "
		"\"power\": \"%s\", \"integrationTime\": %.1f, "
		"\"filterPressure\": %.2f, \"maxPressure\": %.2f, "
		"\"temperature\": %.1f, \"batteryVoltage\": %.1f, "
		"\"depth\": %.2f, \"leak\": %d, \"location\": \"%s\"," 
		"\"serialNumber\": \"%s\", \"deploymentLabel\": \"%s\"," 
		"\"versionNumber\": \"%s\", \"logLevel\": \"%s\"}",

		hwStatus.dateTimeString().c_str(), scriptInterp.getCycleNumber(),
		scriptInterp.getCurrentLine(), scriptInterp.samplingEnabled(),
		(config.getHardwareConfig() == Config::BASIC ?
							   "BASIC" : "TWO_REAGENTS"),
		samplePump.getCurrentRate(), referencePump.getCurrentRate(), 
		reagent1Pump.getCurrentRate(), reagent2Pump.getCurrentRate(),
		(int) referencePump.getLevel(true),
		(int) reagent1Pump.getLevel(true),
		(int) reagent2Pump.getLevel(true),
		filterValve.state(),
		Util::bits2string(mixValves.state(), 2).c_str(),
		portValve.state(),
		Util::bits2string(spectrometer.getLights(),3).c_str(),
		(spectrometer.getStatus() ? 1 : 0),
		Util::bits2string(powerControl.get(),2).c_str(),
		spectrometer.getIntTime(),
		hwStatus.filterPressure(), hwStatus.maxFilterPressure(),
		hwStatus.temperature(), hwStatus.voltage(), hwStatus.depth(), hwStatus.leak(),
		locationSensor.getRecordedLocation().toString().c_str(),
		serialNumber.c_str(), config.getDeploymentLabel().c_str(),
		versionNumber.c_str(), 
		logger.logLevel2string(console.getLevel()).c_str());
	console.reply(buf);
}

void ConsoleInterp::pumpControl(vector<string>& words) {
	if (calibrationInProgress &&
		(words[0] != pumpInCalibration || words[1] != "calibrateFinish")) {
		reply("command not allowed while calibration in progress");
		return;
	}

	if (powerControl.get() != 0b11) powerControl.on();

	Pump *pump;
		 if (words[0] == "samplePump")	  pump = &samplePump;
	else if (words[0] == "referencePump") pump = &referencePump;
	else if (words[0] == "reagent1Pump")  pump = &reagent1Pump;
	else if (words[0] == "reagent2Pump")  pump = &reagent2Pump;
	else {
		reply("do not recognize command: " + words[0]);
		return;
	}
	string name = pump->getName();

	if (words.size() == 1) {
		char buf[100];
		snprintf(buf, 100, "%s variables: %.2f, %.2f",
			name.c_str(), pump->getCurrentRate(),
			pump->getMaxRate());
		reply(buf);
		return;
	} else if (scriptInterp.samplingEnabled()) {
		reply("cannot perform this operation while automated "
			  "sampling is enabled");
		return;
	}

	if (words[1] == "on") {
		if (words.size() < 3) {
			reply("missing pump rate argument");
			return;
		}
		double rate = strtod(words[2].c_str(),0);
		pump->on(rate);
		char buf[100];
		snprintf(buf, 100, "turning on %s at rate %.2f ml/m",
			 	 name.c_str(), pump->getCurrentRate());
		reply(buf);
	} else if (words[1] == "off") {
		reply("turning off " + name);
		pump->off();
	} else if (words[1] == "calibrateStart") {
		// rely on user to set valves
		reply("starting " + name + " calibration, "
			  "click again when 10 ml pumped\n");
		pump->on(pump->getMaxRate());
		calibrationT0 = Util::elapsedTime();
		calibrationInProgress = true;
		pumpInCalibration = name;
	} else if (words[1] == "calibrateFinish") {
		pump->off();
		double finish = Util::elapsedTime();
		double rate = 10.0 / (finish - calibrationT0); // ml/s
		rate *= 60;  // now, in ml/m
		if (rate > 10.0) {
			logger.error("computed rate too high, try again");
			rate = 10.0;
		}
		char buf[200];
		snprintf(buf, sizeof(buf), "setting maxRate for %s to "
			 " %.2f ml/m\n", name.c_str(), rate);
		reply(buf);
		pump->setMaxRate(rate);
		calibrationInProgress = false;
	} else {
		reply("unrecognized argument: " + words[1]);
	}
}

void ConsoleInterp::valveControl(vector<string>& words) {
	if (powerControl.get() != 0b11) powerControl.on();

	if (words.size() == 1) {
		string s;
		if (words[0] == "filterValve") {
			s = Util::bits2string(filterValve.state(),1);
		} else if (words[0] == "mixValves") {
			s = Util::bits2string(mixValves.state(),2);
		} else if (words[0] == "portValve") {
			s = Util::bits2string(portValve.state(),1);
		} else {
			reply("do not recognize command: " + words[0]);
			return;
		}
		reply("state is " + s);
		return;
	}

	if (scriptInterp.samplingEnabled()) {
		reply("cannot perform this operation while "
				  "automated sampling is enabled");
		return;
	}
	int x = Util::string2bits(words[1]);
	reply("setting valve state (" + words[0] + ") to " + words[1]);
	if (words[0] == "filterValve") {
		filterValve.select(x);
	} else if (words[0] == "mixValves") {
		mixValves.select(x >> 1, x & 1);
	} else if (words[0] == "portValve") {
		portValve.select(x);
	} else {
		reply("do not recognize command: " + words[0]);
		return;
	}
}

void ConsoleInterp::fluidSupplyControl(vector<string>& words) {
	double level;
	if (words.size() == 1) {
		if (words[0] == "referenceSupply") {
			level = referencePump.getLevel();
		} else if (words[0] == "reagent1Supply") {
			level = reagent1Pump.getLevel();
		} else if (words[0] == "reagent2Supply") {
			level = reagent2Pump.getLevel();
		} else {
			reply("do not recognize command: " + words[0]);
			return;
		}
		reply("fluid level is " + to_string((int) level) +
				  " ml");
		return;
	}
	if (scriptInterp.samplingEnabled()) {
		reply("cannot perform this operation while "
				  "automated sampling is enabled");
		return;
	}

	level = strtod(words[1].c_str(),0);
	if (words[0] == "referenceSupply") {
		referencePump.setLevel(level);
		level = referencePump.getLevel();
	} else if (words[0] == "reagent1Supply") {
		reagent1Pump.setLevel(level);
		level = reagent1Pump.getLevel();
	} else if (words[0] == "reagent2Supply") {
		reagent2Pump.setLevel(level);
		level = reagent2Pump.getLevel();
	} else {
		reply("do not recognize command: " + words[0]);
		return;
	}
	reply("setting " + words[0] + " fluid level to " +
		  to_string((int) level) + " ml");
}

} // ends namespace
