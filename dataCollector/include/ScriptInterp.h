/** \file ScriptInterp.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef SCRIPTINTERP_H
#define SCRIPTINTERP_H

#include "stdinc.h" 
#include <mutex> 
#include <condition_variable> 
#include "Util.h"
#include "Interrupt.h"
#include "DataStore.h"
#include "Exceptions.h"
#include "CollectorState.h"

using namespace std;

namespace fizz {

extern CollectorState cstate;

/** Interpreter for automated sample collection script.
 *
 *  Script is read from a file during initialization, checked for
 *  common syntax errors and converted to an internal format.
 *  Calling the run method starts the script thread running. The script
 *  is run repeatedly with a specified wait time between successive
 *  runs. Each run constitutes a single sample cycle. While waiting
 *  between cycles, the system runs in a reduced power mode.

 *  Script format. Each line of the script contains a command to
 *  perform some operation. A command consists of a command name
 *  and zero or more arguments (separated by spaces). Indentation
 *  is used to identify groups of commands for conditional execution
 *  and looping.
 *  
 *  An example script is shown below. This example reproduces the
 *  standard data collection steps for the original OPD.
 *  The # sign is used to introduce a comment. 
 *  The given comments explain what each command does.
 *
 *  run 100 240	
 *  	# run directive specifies 100 cycles, 240 seconds between cycles
 *  	# this directive must appear before any other script commands;
 *  	# if cycle count is zero, runs forever; if run directive is
 *  	# omitted, both arguments default to zero
 *  on 1 8
 *  	# execute the following commands on the first cycle of each 
 *  	# successive group of eight cycles
 *  	announce starting reference phase
 *  		# announcements are echoed to console when script runs
 *  	referenceSample 4 2
 *  		# fill spectrometer with reference sample
 *  		# pump 4 ml at rate of 2 ml per minute
 *	optimizeIntegrationTime
 *  		# adjust spectrometer integration time to
 *  		# maximize sensitivity
 *  	getDark dark
 *  		# obtain a dark spectrum from the spectrometer and save it
 *  		# in an output data file; assign it the label dark
 *  	checkLights
 *  		# verify that the spectrometer lights are working
 *  	getSpectrum reference dark
 *  		# obtain a spectrum from the spectrometer and save it
 *  		# in the data file; label it reference and link it to most
 *  		# recent spectrum labeled dark
 *
 *  announce starting filtered sample phase
 *  filteredSample 4 2 0 0
 *  	# fill spectrometer with a filtered seawater sample,
 *  	# possibly mixed with reagent1 and/or reagent2;
 *  	# the first argument specifies the total volume to be pumped (4 ml)
 *  	# the second argument specifies the pump rate (in ml per minute)
 *  	# the third argument specifies the ratio of the volume of reagent1
 *  	# to the volume of filtered seawater;
 *  	# the fourth argument specifies the ratio of the volume of reagent2
 *  	# to the volume of filtered seawater;
 *  	# note: the last two arguments are optional and default to 0
 *  getDark dark
 *  getSpectrum cdom dark reference
 *  	# obtain a spectrum from the spectrometer and save it;
 *  	# label it cdom and link to most recent spectrum labeled dark,
 *  	# and most recent spectrum labeled reference
 *  announce starting unfiltered sample phase
 *  unfilteredSample 7 1 0 0
 *  	# fill spectrometer with an unfiltered seawater sample
 *  	# run the pump for 7 seconds at rate 1 ml/s;
 *  	# the last two (optional) arguments specify the fraction of the
 *  	# total sample for reagents1 and 2; default values are 0
 *  getSpectrum disc dark cdom
 *  	# obtain a spectrum from the spectrometer and save it;
 *  	# include link to most recent dark spectrum and most recent
 *  	# spectrum of filtered seawater
 *  repeat 2
 *  	# repeat the following commands 2 times
 *  	pause .5	# pause for 0.5 seconds
 *  	getSpectrum disc dark cdom;
 */
class ScriptInterp {
public:		ScriptInterp();
	void	initState();

	enum sampleType {
		Reference=1, Unfiltered=2, Filtered=3
	};
	enum opIndex {
		Nil=0, On=1, Announce=2, Repeat=3, RepeatEnd=4, Pause=5,
		ReferenceSample=6, UnfilteredSample=7,
		FilteredSample=8, FilteredSampleAdaptive=9,
		GetSpectrum=10, GetDark=11, CheckLights=12, 
		Lights=14, OptimizeIntTime=15, RecordDepth=16,
		RecordLocation=17
	};

	int		readScript(const string&);
	bool	parseLine(vector<string>&, string&, int);

	void	begin();
	void	end();
	void	join();

	void	start();
	void	stop();
	void	resume();
	bool	samplingEnabled();

	string	getScriptString() { return scriptString; };

	int		getCurrentLine() { return currentLine; }
	long	getCycleNumber() { return cycleNumber; }
	void	setCycleNumber(long c) {
			cycleNumber = c; cstate.setCycleNumber(c);
	}
	bool	zombie() { return zombieFlag; }

private:
	void	run();
	long	nextCycleDelay();
	void	sampleCycle(int);

	string	scriptString;

	/** Data structure that represents a script command.
	 *  The anonymous union includes a struct for each distinct command.
	 */
	class Command {
		public:
		opIndex	op;
		int line;	// line from original script
		union {
		struct { long step, period, nextStep; } on;
		struct { string *line; } announce;
		struct { long limit, count, nextStep; } repeat;
		struct { long firstStep; } repeatEnd;
		struct { double delay; } pause;
		struct { double volume, refPumpRate, samplePumpRate; } refSample; 
		struct { double volume, pumpRate, frac1, frac2; } unfSample; 
		struct { double volume, pumpRate, frac1, frac2; } filSample; 
		struct { double volume, frac1, frac2; } fsaSample; 
		struct { string *label, *prereq1label, *prereq2label;}
			getSpectrum;
		struct { string *label; } getDark; 
		struct { char noargs; } checkLights; 
		struct { char noargs; } recordDepth; 
		struct { char noargs; } recordLocation; 
		struct { int lightConfig; } lights; 
		struct { double volume, refPumpRate, samplePumpRate; }
			optimizeIntTime; 
		};
		Command() : op(Nil) {
		}
		Command(opIndex op, int line) : op(op), line(line) {
			if (op == Announce) announce.line = new string();
			else if (op == GetDark) getDark.label = new string();
			else if (op == GetSpectrum) {
				getSpectrum.label = new string();
				getSpectrum.prereq1label = new string();
				getSpectrum.prereq2label = new string();
			}
		};
		Command(const Command& x) {
			op = x.op; line = x.line;
			switch (op) {
			case On: on = x.on; break; 
			case Announce:
				announce.line = new string(*(x.announce.line));
				break; 
			case Repeat: repeat = x.repeat; break; 
			case RepeatEnd: repeatEnd = x.repeatEnd; break; 
			case Pause: pause = x.pause; break; 
			case ReferenceSample: refSample = x.refSample; break; 
			case FilteredSample: filSample = x.filSample; break; 
			case UnfilteredSample: unfSample = x.unfSample; break; 
			case FilteredSampleAdaptive:
				fsaSample = x.fsaSample; break; 
			case GetDark:
				getDark.label = new string(*(x.getDark.label));
				break; 
			case GetSpectrum:
				getSpectrum.label
				    = new string(*(x.getSpectrum.label));
				getSpectrum.prereq1label
				    = new string(*(x.getSpectrum.prereq1label));
				getSpectrum.prereq2label
				    = new string(*(x.getSpectrum.prereq2label));
				break; 
			case CheckLights: checkLights = x.checkLights; break; 
			case RecordDepth: recordDepth = x.recordDepth; break; 
			case RecordLocation: recordLocation = x.recordLocation; break; 
			case Lights: lights = x.lights; break;
			case OptimizeIntTime: 
				optimizeIntTime = x.optimizeIntTime;
				break; 
			default: break;
			}
		};
		~Command() {
			if (op == Announce) delete announce.line;
			else if (op == GetDark) delete getDark.label;
			else if (op == GetSpectrum) {
				delete getSpectrum.label;
				delete getSpectrum.prereq1label;
				delete getSpectrum.prereq2label;
			}
		};
		string toString();
	};
	vector<Command> script;		///< internal representation

	long	cycleNumber;		///< current sample cycle number
	long	maxCycleCount;		///< number of sampling cycles
	long	interCyclePeriod;	///< number of minutes between cycles
	int	currentLine;			///< line of script being executed

	bool	quitFlag;			///< used to shutdown thread
	bool	zombieFlag;			///< set when waiting for thread to be killed
	bool	autoRun;			///< set in autoRun mode

	mutex	mtx;				///< mutual exclusion while starting/stopping
	thread	myThread;			///< thread for automated sampling
	thread::id thread_id;

	static	void startThread(ScriptInterp&);
};

/** Create string representation of a script command.
 *  @param cmd is the internal representation of a script command.
 *  @return a string representing the command.
 */
inline string ScriptInterp::Command::toString() {
	stringstream ss;
	switch (op) {
	case On:
		ss << "on " << on.step << " " << on.period;
		break;
	case Announce:
		ss << "announce " << *announce.line; break;
	case Repeat:
		ss << "repeat " << repeat.limit; break;
	case RepeatEnd:
		ss << "repeatEnd " << repeatEnd.firstStep; break;
	case Pause:
		ss << "pause " << pause.delay; break;
	case ReferenceSample:
		ss << "referenceSample " << refSample.volume
		   << " " << refSample.refPumpRate
		   << " " << refSample.samplePumpRate;
		break;
	case UnfilteredSample:
		ss << "unfilteredSample " << unfSample.volume << " "
		   << unfSample.pumpRate << " "
		   << unfSample.frac1 << " " << unfSample.frac2;
		 break;
	case FilteredSample:
		ss << "filteredSample " << filSample.volume << " "
		   << filSample.pumpRate << " "
		   << filSample.frac1 << " " << filSample.frac2;
		 break;
	case FilteredSampleAdaptive:
		ss << "filteredSampleAdaptive " << fsaSample.volume << " "
		   << fsaSample.frac1 << " " << fsaSample.frac2;
		break;
	case GetSpectrum:
		ss << "GetSpectrum " << *(getSpectrum.label) << " "
		   << *(getSpectrum.prereq1label) << " "
		   << *(getSpectrum.prereq2label);
		break;
	case GetDark:
		ss << "GetDark " << *(getDark.label); break;
	case RecordDepth:
		ss << "RecordDepth"; break;
	case RecordLocation:
		ss << "RecordLocation"; break;
	case CheckLights:
		ss << "CheckLights"; break;
	case Lights:
		ss << "Lights " << (lights.lightConfig & 2 ? "1" : "0")
				<< (lights.lightConfig & 1 ? "1" : "0");
		break;
	case OptimizeIntTime:
		ss << "optimizeInterationTime " << optimizeIntTime.volume
		   << " " << optimizeIntTime.refPumpRate
		   << " " << optimizeIntTime.samplePumpRate;
		break;
	case Nil:
		ss << "Nil"; break;
	default:
		ss << "unrecognized command"; break;
	}
	return ss.str();
}

} // ends namespace

#endif
