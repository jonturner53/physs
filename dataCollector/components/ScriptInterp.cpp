/** @file ScriptInterp.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Util.h"

#include "Pump.h"
#include "SupplyPump.h"
#include "Valve.h"
#include "MixValves.h"
#include "LocationSensor.h"
#include "Status.h"
#include "Spectrometer.h"
#include "CommLink.h"
#include "PowerControl.h"

#include "Config.h"
#include "MaintLog.h"
#include "CollectorState.h"
#include "Console.h"
#include "ConsoleInterp.h"
#include "DataStore.h"
#include "ScriptInterp.h"
#include "Operations.h"
#include "Interrupt.h"

using std::this_thread::sleep_for;

namespace fizz {

extern Logger logger;
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
extern DataStore dataStore;
extern Interrupt interrupt;

extern Arduino arduino;

/** Constructor for ScriptInterp object.
 */
ScriptInterp::ScriptInterp() {
	cycleNumber = 1;
	currentLine = 0;
	zombieFlag = false;
}

/** Load state variables.
 *  Should only be called from main thread before other threads start running.
 */
void ScriptInterp::initState() {
	cycleNumber = cstate.getCycleNumber();
}

/** Read the script file, check syntax and save in the script vector.
 *  Called from consoleInterp thread.
 *  @return -1 if unable to open file, line number where syntax error was
 *  detected or 0 if no errors.
 */
int ScriptInterp::readScript(const string& scriptFileName) {
	ifstream scriptFile;

	scriptFile.open(scriptFileName.c_str(), ifstream::in);
	if (scriptFile.fail()) {
		logger.error("cannot open script file: %s",
			     scriptFileName.c_str());
		return -1;
	}
	script.clear();		// required when re-starting script

	logger.info("ScriptInterp: opened %s", scriptFileName.c_str());
	const int maxDepth = 100;
	vector<pair<int,int>> parseStack(maxDepth);
	int top = 0;		 // first unused slot on stack;
	int indent = 0;	  	// current number of spaces used for indent;

	int lineNumber = 0;
	long scriptStep = 0;
	maxCycleCount = 0;
	interCyclePeriod = 0;
	scriptString.clear();

	while (scriptFile.good()) { 
		string line; vector<string> words;
		getline(scriptFile, line); // removes newline
		lineNumber++;
		scriptString += line + '\n';
		if (line.length() == 0) continue;
		int i = line.find('#');
		if (i >= 0) line.erase(i); // strip comments;
		Util::split(line, 5, words);
		if (words.size() == 0) continue;   // ignore blank lines;

		logger.trace("ScriptInterp: parsing %s", line.c_str());

		// check for run directive - it must come first
		if (words[0].compare("run") == 0) {
			if (scriptStep != 0) {
				scriptFile.close(); return lineNumber;
			}
			maxCycleCount = strtol(words[1].c_str(),0,0);
			interCyclePeriod = strtol(words[2].c_str(),0,0);
			continue;
		}

		// parse line of input, returning if exception thrown
		try { 
			if (!parseLine(words, line, lineNumber)) {
				scriptFile.close(); return lineNumber;
			}
		} catch (...) {
			scriptFile.close(); return lineNumber;
		}

		logger.trace("ScriptInterp: script[%ld]=%s", scriptStep,
			     script[scriptStep].toString().c_str());
  
		i = line.find(words[0]);
		logger.trace("handling indent new=%d old=%d", i, indent);
		if (i > indent) {
			if (top > maxDepth - 1 || scriptStep == 0 ||
				  (script[scriptStep-1].op != On and 
				   script[scriptStep-1].op != Repeat)) {
				scriptFile.close(); return lineNumber;
			}
			parseStack[top].first = indent;
			parseStack[top].second = scriptStep-1;
				// stack record contains number of spaces for 
				// indent, and the index of the previous 
				// scriptStep; (either an "on" command, or 
				// a "repeat" command);
			indent = i;
			top++;
			logger.trace("ScriptInterp: increasing indent");
		} else if (i < indent) {
			top--;
			if (top < 0 || script[scriptStep-1].op == On ||
				       script[scriptStep-1].op == Repeat ||
				       parseStack[top].first != i) {
				scriptFile.close(); return lineNumber;
			}
			indent = parseStack[top].first;
			int step = parseStack[top].second;
			if (script[step].op == On) {
				script[step].on.nextStep = scriptStep;
					// add current scriptStep index to
					// on command; allows us to jump past
					// intervening commands when
					// on-condition is not true
			} else if (script[step].op == Repeat) {
				Command cmd(RepeatEnd, 0);
				cmd.repeatEnd.firstStep = step;
				script.insert(script.end()-1, cmd);
				logger.trace("ScriptInterp: script[%d]=%s",
					scriptStep,
					script[scriptStep].toString().c_str());
				scriptStep++;
				logger.trace("ScriptInterp: script[%d]=%s",
					scriptStep,
					script[scriptStep].toString().c_str());
				script[step].repeat.nextStep = scriptStep;
					// add current scriptStep index to
					// repeat command; allows us to jump 
					// past body of loop when loop counter
					// reaches limit
			}
			logger.trace("ScriptInterp: decreasing indent");
		} else { // no change to indent;
			// previous command must not be "on" or "repeat";
			if (scriptStep != 0 &&
				(script[scriptStep-1].op == On ||
				script[scriptStep-1].op == Repeat)) {
				scriptFile.close(); return lineNumber;
			}
		}
		scriptStep++;
	}

	// unwind stack at end of script;
	while (top > 0) {	
		top--;
		if (script[scriptStep-1].op == On ||
		    script[scriptStep-1].op == Repeat) {
			scriptFile.close(); return lineNumber;
		}
		indent = parseStack[top].first;
		int step = parseStack[top].second;
		if (script[step].op == On) {
			script[step].on.nextStep = scriptStep;
				// add current scriptStep index to
				// on command; allows us to jump past
				// intervening commands when
				// on-condition is not true
		} else if (script[step].op == Repeat) {
			Command cmd(RepeatEnd, 0);
			cmd.repeatEnd.firstStep = step;
			script.push_back(cmd);
			logger.trace("ScriptInterp: script[%d]=%s", scriptStep,
				     script[scriptStep].toString().c_str());
			scriptStep++;
			script[step].repeat.nextStep = scriptStep;
		}
		logger.trace("ScriptInterp: decreasing indent");
	}
	// add dummy pause to end of script
	Command cmd(Pause, 0);
	cmd.pause.delay = 0.;
	script.push_back(cmd);

	scriptFile.close();
	logger.details("successfully parsed script");
	return 0;
}

/** Parse one line of the script and append internal representation 
 *  to script vector.
 *  @param words is a non-empty vector of words representing
 *  a single script command.
 *  @param line is the original string version of line.
 *  @param lineNum is the line number for the script line
 *  @param return false if error detected, else true.
 */
bool ScriptInterp::parseLine(vector<string>& words, string& line, int lineNum) {
	if (script.size() == script.capacity())
		script.reserve(2*script.size());
	if (words[0].compare("on") == 0) {
		long step = strtol(words[1].c_str(),0,0);
		long period = strtol(words[2].c_str(),0,0);
		if (step < 1 || step > period || period < 1) return false;
		Command cmd(On, lineNum);
		cmd.on.step = step;
		cmd.on.period = period;
		script.push_back(cmd);
	} else if (words[0].compare("announce") == 0) {
		string suffix;
		unsigned int i = line.find("announce");
		suffix.assign(line, i+8, string::npos);
		suffix.erase(0,suffix.find_first_not_of(" \t\v\r\n\f"));
		Command cmd(Announce, lineNum);
		cmd.announce.line->assign(suffix);
		script.push_back(cmd);
	} else if (words[0].compare("repeat") == 0) {
		long limit = strtol(words[1].c_str(),0,0);
		if (limit < 1) return false;
		Command cmd(Repeat, lineNum);
		cmd.repeat.limit = limit;
		cmd.repeat.count = 0;
		script.push_back(cmd);
	} else if (words[0].compare("pause") == 0) {
		double delay = strtod(words[1].c_str(),0);
		Command cmd(Pause, lineNum);
		cmd.pause.delay = delay;
		script.push_back(cmd);
	} else if (words[0].compare("referenceSample") == 0) {
		double volume = 2;
		double refPumpRate = referencePump.getMaxRate();
		double samplePumpRate = 2;
		if (words.size() > 1) volume = strtod(words[1].c_str(),0);
		if (words.size() > 2) refPumpRate = strtod(words[2].c_str(),0);
		if (words.size() > 3) samplePumpRate = strtod(words[3].c_str(),0);
		Command cmd(ReferenceSample, lineNum);
		cmd.refSample.volume = volume;
		cmd.refSample.refPumpRate = refPumpRate;
		cmd.refSample.samplePumpRate = samplePumpRate;
		script.push_back(cmd);
	} else if (words[0].compare("unfilteredSample") == 0) {
		double volume = 10; double pumpRate = 2;
		double frac1 = 0; double frac2 = 0;
		if (words.size() > 1) volume = strtod(words[1].c_str(),0);
		if (words.size() > 2) pumpRate = strtod(words[2].c_str(),0);
		if (words.size() > 3) frac1 = strtod(words[3].c_str(),0);
		if (words.size() > 4) frac2 = strtod(words[4].c_str(),0);
		Command cmd(UnfilteredSample, lineNum);
		cmd.unfSample.volume = volume;
		cmd.unfSample.pumpRate = pumpRate;
		cmd.unfSample.frac1 = frac1;
		cmd.unfSample.frac2 = frac2;
		script.push_back(cmd);
	} else if (words[0].compare("filteredSample") == 0) {
		double volume = 10;
		double pumpRate = 2;
		double frac1 = 0; double frac2 = 0;
		if (words.size() > 1) volume = strtod(words[1].c_str(),0);
		if (words.size() > 2) pumpRate = strtod(words[2].c_str(),0);
		if (words.size() > 3) frac1 = strtod(words[3].c_str(),0);
		if (words.size() > 4) frac2 = strtod(words[4].c_str(),0);
		Command cmd(FilteredSample, lineNum);
		cmd.filSample.volume = volume;
		cmd.filSample.pumpRate = pumpRate;
		cmd.filSample.frac1 = frac1;
		cmd.filSample.frac2 = frac2;
		script.push_back(cmd);
	} else if (words[0].compare("filteredSampleAdaptive") == 0) {
		double volume = 10;
		double frac1 = 0; double frac2 = 0;
		if (words.size() > 1) volume = strtod(words[1].c_str(),0);
		if (words.size() > 2) frac1 = strtod(words[2].c_str(),0);
		if (words.size() > 3) frac2 = strtod(words[3].c_str(),0);
		Command cmd(FilteredSampleAdaptive, lineNum);
		cmd.fsaSample.volume = volume;
		cmd.fsaSample.frac1 = frac1;
		cmd.fsaSample.frac2 = frac2;
		script.push_back(cmd);
	} else if (words[0].compare("getSpectrum") == 0) {
		if (words.size() < 2) return false;
		Command cmd(GetSpectrum, lineNum);
		cmd.getSpectrum.label->assign(words[1]);
		cmd.getSpectrum.prereq1label->assign(
			words.size()<3 ? string("") : words[2]);
		cmd.getSpectrum.prereq2label->assign(
			words.size()<4 ? string("") : words[3]);
		script.push_back(cmd);
	} else if (words[0].compare("getDark") == 0) {
		if (words.size() < 2) return false;
		Command cmd(GetDark, lineNum);
		cmd.getDark.label->assign(words[1]);
		script.push_back(cmd);
	} else if (words[0].compare("recordDepth") == 0) {
		Command cmd(RecordDepth, lineNum);
		script.push_back(cmd);
	} else if (words[0].compare("recordLocation") == 0) {
		Command cmd(RecordLocation, lineNum);
		script.push_back(cmd);
	} else if (words[0].compare("checkLights") == 0) {
		Command cmd(CheckLights, lineNum);
		script.push_back(cmd);
	} else if (words[0].compare("lights") == 0) {
		Command cmd(Lights, lineNum);
		cmd.lights.lightConfig = 
			(words[1][0] == '0' ? 0 : 4) +
			(words[1][1] == '0' ? 0 : 2) +
			(words[1][2] == '0' ? 0 : 1);
		script.push_back(cmd);
	} else if (words[0].compare("optimizeIntegrationTime") == 0) {
		double volume = 2;
		double refPumpRate = 4;
		double samplePumpRate = 2;
		if (words.size() > 1) volume = strtod(words[1].c_str(),0);
		if (words.size() > 2) refPumpRate = strtod(words[2].c_str(),0);
		if (words.size() > 3) samplePumpRate = strtod(words[3].c_str(),0);
		Command cmd(OptimizeIntTime, lineNum);
		cmd.refSample.volume = volume;
		cmd.refSample.refPumpRate = refPumpRate;
		cmd.refSample.samplePumpRate = samplePumpRate;
		script.push_back(cmd);
	} else {
		return false;
	}
	return true;
}

//
// Methods used by main thread to initiate/terminate scriptInterp thread.
//


/** Perform operations in response to an interrupt request.
 *  Turns of pumps, releases valves and turns off power
 *  to pumps, valves and lights.
 */
void scriptIntHandler() {
	Operations::idleMode(); powerControl.off();
		// note communication is not disabled
}

/** Start the scriptInterpreter thread running. 
 *  This method is called from the main thread.
 */
void ScriptInterp::begin() {
	unique_lock<mutex> lck(mtx);
    logger.details("ScriptInterp: starting thread");
	myThread = thread(startThread, ref(*this));
	thread_id = myThread.get_id();
	interrupt.register_client(thread_id, "script interpreter",
							  &scriptIntHandler);
}

/** Work-around used to initiate thread execution.
 *  Compiler won't accept code that starts thread with
 *  direct invocation of ScriptInterp::run.
 *  This gets around the problem.
 */
void ScriptInterp::startThread(ScriptInterp& si) {
	si.run();
}

/** Stop the scriptInterp thread.
 *  Sets quitFlag and disables scriptInterp thread.
 *  Caller must still wait for thread to terminate itself using join().
 *  This method is called from the main thread.
 */
void ScriptInterp::end() {
	unique_lock<mutex> lck(mtx);
    logger.details("ScriptInterp: terminating thread");
    quitFlag = true;
	interrupt.request(thread_id, true);
}

/** Wait for the consoleInterp thread to finish.
 *  Called from main thread.
 *  Return immediately if the thread is not joinable.
 */
void ScriptInterp::join() {
	if (myThread.joinable()) myThread.join();
}

//
// Methods used by consoleInterp thread to interrupt/start/resume 
// sample collection by scriptInterp thread.
//

/** Suspend the sample collection thread.
 *  Called from consoleInterp thread.
 *  Returns once the thread has suspended itself.
 */
void ScriptInterp::stop() {
    unique_lock<mutex> lck(mtx);
    logger.details("ScriptInterp: suspending sample collection");
	if (!interrupt.inProgress(thread_id)) interrupt.request(thread_id);
}

/** Resume execution of the sample collection thread.
 *  Called from consoleInterp thread. Allows scriptInterp
 *  to proceed with its execution.
 */
void ScriptInterp::resume() {
	unique_lock<mutex> lck(mtx);
    logger.details("ScriptInterp: resuming sample collection");
	interrupt.clear(thread_id);
}

/** Restart execution of the sample collection thread.
 *  Called from consoleInterp thread.
 *  Thread starts from the beginning of the sample collection
 *  process (that is, it starts again at cycle 1).
 */
void ScriptInterp::start() {
    unique_lock<mutex> lck(mtx);
    logger.details("ScriptInterp: starting sample collection");
	setCycleNumber(1); interrupt.clear(thread_id);
}

/** Return true if sampling is turned on, else false.
 */
bool ScriptInterp::samplingEnabled() {
    unique_lock<mutex> lck(mtx);
	return !interrupt.inProgress(thread_id);
}

/** Run the script repeatedly, sleeping between sample cycles.  */
void ScriptInterp::run() {
	// wait to begin sample collection
	try { interrupt.selfInterrupt(); } catch(...) {}

	arduino.log();

	int failedCycleCount = 0;
	while (!quitFlag) {
		powerControl.on();
		if (dataStore.getSpectrumCount() > 2000) {
			setCycleNumber(1);
		}
		if (cycleNumber > maxCycleCount && maxCycleCount != 0) {
			// done collecting samples
			dataStore.close();
			logger.info("ScriptInterp: completed automated sampling");
			Operations::idleMode();
			try { interrupt.selfInterrupt(); } catch(...) {}
			continue;
		}

		try {
			portValve.select(
				config.getPortSwitching() ? (cycleNumber & 1) : 0);
			if (cycleNumber == 1) {
				Operations::purgeBubbles();
				dataStore.saveDeploymentRecord();
				dataStore.saveConfigRecord();
				dataStore.saveScriptRecord();
				dataStore.saveMaintLogRecord();
			}
			sampleCycle(cycleNumber);
			portValve.select(
				config.getPortSwitching() ? ((cycleNumber+1) & 1) : 0);
			Operations::flush();
				// flush mixing coils (if present), filter and
				// waveguide; do this using port valve config
				// for next cycle, so that intake tubing does
				// not contain ref fluid at start of next cycle
			setCycleNumber(cycleNumber+1);
				// increment after flush, so that if interrupted
				// during flush, will not advance to next cycle
			failedCycleCount = 0;
		} catch(PressureException& e) {
			Operations::idleMode();
			if (failedCycleCount++ < 10) {
				logger.warning("over-pressure exception, starting cycle");
				dataStore.saveResetRecord();
				continue;
			}
			logger.info("ScriptInterpreter: too many failed "
				    	"cycles, suspending sampling");
			dataStore.close();
			failedCycleCount = 0;
			continue; 
		} catch(InterruptException& e) {
			logger.debug("ScriptInterp: resuming sampling following interrupt");
			continue;
		} catch(exception& e) {
			logger.debug("ScriptInterp: caught %s, suspending sampling",
				     	 e.what()); 
			try { interrupt.selfInterrupt(); } catch(...) {}
			continue;
		}

		// delay until next cycle
		if (interCyclePeriod == 0) continue;

		powerControl.off();
		dataStore.close();

		long delta = nextCycleDelay();
		logger.details("ScriptInterp: going to sleep until next cycle "
					"(%d minutes)", delta);

		// in powerSave mode, tell arduino to shut off processor power
		// for delta minutes, then wait for main to kill scriptThread
		if (config.getPowerSave() && arduino.isReady()) {
			arduino.send("S" + to_string(delta));
				// note: built-in 30 sec delay before power turns off
			arduino.log();
			zombieFlag = true;		// tells main, we're waiting to die
			try { interrupt.selfInterrupt(); } catch(...) {}
			break;
		}

		// otherwise, just wait until it's time to wake up
		// respond to stop signals from console, while waiting
		try {
			Operations::idleMode(); 
			double wakeTime = Util::elapsedTime() + 60. * delta;
			while (Util::elapsedTime() < wakeTime) {
				interrupt.pause(5); arduino.log();
			}
			if (!commLink.isActive()) {
				commLink.enable();
				interrupt.pause(100); // allow time for ssh session to start
			}
			logger.details("ScriptInterp: waking up");
		} catch(InterruptException&) {
			logger.details("ScriptInterp: resuming after interrupted sleep");
		} catch(exception& e) {
			logger.debug("ScriptInterp: caught %s, while sleeping"
						 "suspending sampling", e.what()); 
			try { interrupt.selfInterrupt(); } catch(...) {}
		}
	}
	logger.info("ScriptInterp: quitting sample collection");
	return;
}

/** Compute number of minutes to start of next cycle.
 *  Uses arduino real-time clock if available.
 *  @return number of minutes to next cycle.
 */
long ScriptInterp::nextCycleDelay() {
	string s = hwStatus.dateTimeString();
	// format YYYY-mm-dd hh:mm:ss
	int hours = atoi(s.substr(11,2).c_str());
	int minutes = 60 * hours + atoi(s.substr(14,2).c_str());
	return interCyclePeriod - (minutes % interCyclePeriod);
}

/** Perform a single sample cycle - one pass through the script.
 *  @param cycleNumber is the index of the current cycle.
 *  @return true if cycle completed successfully, else false.
 */
void ScriptInterp::sampleCycle(int cycleNumber) {
	logger.border();
	string dateTime = hwStatus.dateTimeString();
	logger.info("starting cycle %2d at %s", cycleNumber, dateTime.c_str());

	hwStatus.clearMaxFilterPressure(); hwStatus.recordDepth();

	// cycle lights to warmup at start of sample cycle
	spectrometer.setLights(0b111); interrupt.pause(2);
	spectrometer.setLights(0b000); interrupt.pause(2);
	spectrometer.setLights(0b111); interrupt.pause(2);
	spectrometer.setLights(0b000); interrupt.pause(2);

	// execute the script
	unsigned int step = 0;
	while (step < script.size()) {
		Command& cmd = script[step];
		if (cmd.line > 0) currentLine = cmd.line;
		int nextStep = step + 1;

		if (!samplingEnabled()) {
			dataStore.close();
			logger.details("throwing exception in sampleCycle");
			throw InterruptException();
		}

		logger.trace("ScriptInterp::sampleCycle: %s",
			     cmd.toString().c_str());
//cerr << "command: " << cmd.toString() << endl;
		if (cmd.op == On) {
			if (cmd.on.step % cmd.on.period !=
			    cycleNumber % cmd.on.period)
				nextStep = cmd.on.nextStep;
		} else if (cmd.op == Announce) {
			console.logMessage(*cmd.announce.line + "\n");
		} else if (cmd.op == Repeat) {
			if (cmd.repeat.count == 0) {
				cmd.repeat.count = 1;
			} else if (cmd.repeat.count < cmd.repeat.limit) {
				cmd.repeat.count++;
			} else {
				cmd.repeat.count = 0;
				nextStep = cmd.repeat.nextStep;
			}
		} else if (cmd.op == RepeatEnd) {
			nextStep = cmd.repeatEnd.firstStep;
		} else if (cmd.op == ReferenceSample) {
			Operations::referenceSample(cmd.refSample.volume,
						    cmd.refSample.refPumpRate,
							cmd.refSample.samplePumpRate);
		} else if (cmd.op == FilteredSample) {
			Operations::filteredSample(cmd.filSample.volume,
				cmd.filSample.pumpRate, cmd.filSample.frac1,
				cmd.filSample.frac2);
		} else if (cmd.op == FilteredSampleAdaptive) {
			Operations::filteredSampleAdaptive(cmd.fsaSample.volume,
		  		cmd.fsaSample.frac1, cmd.fsaSample.frac2);
		} else if (cmd.op == UnfilteredSample) {
			Operations::unfilteredSample(cmd.unfSample.volume,
				 cmd.unfSample.pumpRate, cmd.unfSample.frac1,
				 cmd.unfSample.frac2);
		} else if (cmd.op == GetDark) {
			spectrometer.getSpectrum(0b110);
			vector<double>& spect = spectrometer.spectrum;
			dataStore.saveSpectrumRecord(spect, *cmd.getDark.label);
		} else if (cmd.op == GetSpectrum) {
			spectrometer.getSpectrum(0b111);
			vector<double>& spect = spectrometer.spectrum;
			dataStore.saveSpectrumRecord(spect,
				*cmd.getSpectrum.label,
				*cmd.getSpectrum.prereq1label,
				*cmd.getSpectrum.prereq2label);
		} else if (cmd.op == CheckLights) {
			bool lightStatus = spectrometer.checkLights();
			if (!lightStatus)
				logger.warning("light failure");
		} else if (cmd.op == Pause) {
			if (cmd.pause.delay > 0)
				interrupt.pause(cmd.pause.delay);
		} else if (cmd.op == RecordDepth) {
			hwStatus.recordDepth();
		} else if (cmd.op == RecordLocation) {
			locationSensor.recordLocation();
		} else if (cmd.op == Lights) {
			spectrometer.setLights(cmd.lights.lightConfig);
		} else if (cmd.op == OptimizeIntTime) {
			Operations::optimizeIntegrationTime(
				cmd.optimizeIntTime.volume,
				cmd.optimizeIntTime.refPumpRate,
				cmd.optimizeIntTime.samplePumpRate);
		}
		step = nextStep;
		arduino.log();
	}
	logger.info("ending cycle %2d at %s", cycleNumber,
		    hwStatus.dateTimeString().c_str());
	currentLine = 0;  // end of cycle, no longer in script
	// send key status information to logger and the dataStore
	logger.info("temp: %.0fC, battery: %.1fV, pressure: %.1fpsi, "
		    "integ time: %.2fms",
			hwStatus.temperature(), hwStatus.voltage(),
			hwStatus.maxFilterPressure(), spectrometer.getIntTime());
	dataStore.saveCycleSummary();
	logger.border();
}

} // ends namespace
