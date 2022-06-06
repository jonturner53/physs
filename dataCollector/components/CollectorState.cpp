/** @file CollectorState.cpp 
 *
 *  @author Jon Turner
 *  @date 2017

 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "CollectorState.h"

#include "Pump.h"
#include "SupplyPump.h"
#include "ConsoleInterp.h"
#include "Config.h"
#include "CollectorState.h"
#include "DataStore.h"

namespace fizz {

extern Pump samplePump;
extern SupplyPump referencePump;
extern SupplyPump reagent1Pump;
extern SupplyPump reagent2Pump;
extern ConsoleInterp consoleInterp;
extern ScriptInterp scriptInterp;
extern Config config;
extern DataStore dataStore;

/** Constructor for CollectorState objects.
 *  @param stateFile name of the state file
 */
CollectorState::CollectorState(const string& stateFile) : stateFile(stateFile) {
	doneReading = false;

	cycleNumber = 0;

	samplePumpMaxRate = 0;
	referencePumpMaxRate = 0;
	reagent1PumpMaxRate = 0;
	reagent2PumpMaxRate = 0;

	referenceSupplyLevel = -1;
	reagent1SupplyLevel = -1;
	reagent2SupplyLevel = -1;

	pressureSensorUpstreamOffset = 0;
	pressureSensorUpstreamScale = 32.0 / 1024;
	pressureSensorDownstreamOffset = 0;
	pressureSensorDownstreamScale = 32.0 / 1024;

	integrationTime = -1;

	currentIndex = 0;
	deploymentIndex = 0;
	spectrumCount = 0;
}

/** Read state file and set internal variables accordingly.
 */
bool CollectorState::read() {
	unique_lock<mutex> lck(cstateMtx);
	if (doneReading) {
		cerr << "CollectorState:: redundant read() call ignored\n";
		return false;
	}
	ifstream ifs;
	ifs.open(stateFile, ifstream::in);
	if (ifs.fail()) {
		cerr << "CollectorState:: cannot open state file\n";
		return false;
	}
	currentIndex = 0; deploymentIndex = 0;
	while (!ifs.eof()) {
		char buf[1024]; buf[1023] = '\0';
		ifs.getline(buf, 1023);

		string line = buf;
		int i = line.find('#');
		if (i >= 0) line.erase(i);

		vector<string> words(3);
		Util::split(line, 3, words);

		if (words.size() == 0) continue;
		if (words.size() != 3 || words[1].compare("=") != 0) {
			cerr << "CollectorState::read: invalid line in "
				"state file: " << line << endl;
			continue;
		}
		
		if (words[0] == "cycleNumber") {
			cycleNumber = atoi(words[2].c_str());

		} else if (words[0] == "samplePumpMaxRate") {
			samplePumpMaxRate = atof(words[2].c_str());
		} else if (words[0] == "referencePumpMaxRate") {
			referencePumpMaxRate = atof(words[2].c_str());
		} else if (words[0] == "reagent1PumpMaxRate") {
			reagent1PumpMaxRate = atof(words[2].c_str());
		} else if (words[0] == "reagent2PumpMaxRate") {
			reagent2PumpMaxRate = atof(words[2].c_str());

		} else if (words[0] == "referenceSupplyLevel") {
			referenceSupplyLevel = atof(words[2].c_str());
		} else if (words[0] == "reagent1SupplyLevel") {
			reagent1SupplyLevel = atof(words[2].c_str());
		} else if (words[0] == "reagent2SupplyLevel") {
			reagent2SupplyLevel = atof(words[2].c_str());

		} else if (words[0] == "pressureSensorUpstreamOffset") {
			pressureSensorUpstreamOffset = atof(words[2].c_str());
		} else if (words[0] == "pressureSensorUpstreamScale") {
			pressureSensorUpstreamScale = atof(words[2].c_str());
		} else if (words[0] == "pressureSensorDownstreamOffset") {
			pressureSensorDownstreamOffset = atof(words[2].c_str());
		} else if (words[0] == "pressureSensorDownstreamScale") {
			pressureSensorDownstreamScale = atof(words[2].c_str());

		} else if (words[0] == "integrationTime") {
			integrationTime = atof(words[2].c_str());

		} else if (words[0] == "currentIndex") {
			currentIndex = atoi(words[2].c_str());
		} else if (words[0] == "deploymentIndex") {
			deploymentIndex = atoi(words[2].c_str());
		} else if (words[0] == "spectrumCount") {
			spectrumCount = atoi(words[2].c_str());
		} else if (words[0] == "recordMap") {
			if (currentIndex == 0 || deploymentIndex == 0) {
				cerr << "CollectorState::read: incomplete "
					"DataStore state\n";
				return false;
			}
			int i = 0; int len = words[2].length();
			while (i < len) {
				int j = words[2].find(" ", i);
				string s;
				if (j < 0) {
					s = words[2].substr(i); i = len;
				} else {
					s = words[2].substr(i,j); i = j+1;
				}
				j = s.find('.');
				if (j < 1 || j > (int) s.length()-2)
					continue;
				string label = s.substr(0,j);
				int index = atoi(s.substr(j+1).c_str());
				recordMap[label] = index;
			}
		} else {
			cerr << "CollectorState::read: invalid line in "
				 "state file: " << line << endl;
			continue;
		}
	}
	ifs.close();
	doneReading = true;
	return true;
}

// Macro used to format floats in output streams
#define FLOAT(x,y) fixed << setprecision(y) << x

/** Save values of internal variables in external state file.
 *  This is only called from setters, so lock is alread engaged.
 */
bool CollectorState::update() {
	if (not doneReading) return false;

	ofstream ofs;
	try {
		ofs.open(stateFile, ifstream::out);
	} catch(...) {
		cerr << "CollectorState::update: cannot open state file\n";
		return false;
	}

	ofs << "cycleNumber = " << cycleNumber << endl;

	ofs << "\n# pump parameters\n";
	ofs << "samplePumpMaxRate = " << FLOAT(samplePumpMaxRate,4) << endl;
	ofs << "referencePumpMaxRate = "
	    << FLOAT(referencePumpMaxRate,4) << endl;
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		ofs << "reagent1PumpMaxRate = "
		    << FLOAT(reagent1PumpMaxRate,4) << endl;
		ofs << "reagent2PumpMaxRate = "
		    << FLOAT(reagent2PumpMaxRate,4) << endl;
	}

	ofs << "\n# supply levels\n";
	ofs << "referenceSupplyLevel = " << FLOAT(referenceSupplyLevel,3)
	    << endl;
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		ofs << "reagent1SupplyLevel = "
		    << FLOAT(reagent1SupplyLevel,3) << endl;
		ofs << "reagent2SupplyLevel = "
		    << FLOAT(reagent2SupplyLevel,3) << endl;
	}

	ofs << "\n# pressure sensor parameters\n";
	ofs << "pressureSensorUpstreamOffset = "
		<< FLOAT(pressureSensorUpstreamOffset,3) << endl
		<< "pressureSensorUpstreamScale = "
		<< FLOAT(pressureSensorUpstreamScale,3) << endl
	    << "pressureSensorDownstreamOffset = "
		<< FLOAT(pressureSensorDownstreamOffset,3) << endl
		<< "pressureSensorDownstreamScale = "
		<< FLOAT(pressureSensorDownstreamScale,3) << endl;

	ofs << "\n# spectrometer parameters\n";
	ofs << "integrationTime = " << integrationTime << endl;
		
	ofs << "\n# data store state\n";
	ofs << "currentIndex = " << currentIndex << endl;
	ofs << "deploymentIndex = " << deploymentIndex << endl;
	ofs << "spectrumCount = " << spectrumCount << endl;
	if (recordMap.size() > 0) {
		ofs << "recordMap =";
		for (auto p = recordMap.begin(); p != recordMap.end(); p++) {
			ofs << " " << p->first << "." << p->second;
		}
	}
	ofs << endl << std::flush;

	ofs.close();
	return true;
}

} // ends namespace

