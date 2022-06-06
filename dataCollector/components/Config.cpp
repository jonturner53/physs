/** @file Config.cpp 
 *
 *  @author Jon Turner
 *  @date 2017

 *  This software was developed for Mote Marine Research Laboratory.
 */

#include <queue>
#include "Config.h"
#include "Logger.h"

namespace fizz {

extern Logger logger;

/** Constructor for Config objects.
 *  @param configFile name of the config file
 */
Config::Config(const string& configFile) : configFile(configFile) {
	waveguideLength = 0.28; // length in meters
	location = { 0.0, 0.0 };
	deploymentLabel = "no label";
	maxPressure = 25.;
	maxDepth = 20.;
	autoRun = -1;
	powerSave = false;
	portSwitching = true;
	ignoreFailures = true;
	logLevelConsole = Logger::DETAILS;
	logLevelStderr = Logger::INFO;
	logLevelDebug = Logger::DEBUG;

	doneReading = false;
}

/** Read config file and set internal variables accordingly.
 */
bool Config::read() {
	logger.debug("Config: reading config file");
	unique_lock<mutex> lck(cfgMtx);
	ifstream ifs;
	ifs.open(configFile, ifstream::in);
	if (ifs.fail()) {
		lck.unlock();
		logger.error("cannot open config file\n");
		return false;
	}
	string cfgStr;
	queue<string> errors;
	while (!ifs.eof()) {
		char buf[1024]; buf[sizeof(buf)-1] = '\0';
		ifs.getline(buf, sizeof(buf));   // discards newline

		string line = buf;
		cfgStr += line + '\n';
		int i = line.find('#');
		if (i >= 0) line.erase(i);

		vector<string> words(3);
		Util::split(line, 3, words);

		if (words.size() == 0) continue;
		if (words.size() != 3 || words[1] != "=") {
			errors.push("invalid line in config file: " + line);
			continue;
		}
		
		if (words[0] == "autoRun") {
			autoRun = atoi(words[2].c_str());
		} else if (words[0] == "hardwareConfig") {
			if (words[2] == "BASIC") {
				hardwareConfig = BASIC;
			} else if (words[2] == "TWO_REAGENTS") {
				hardwareConfig = TWO_REAGENTS;
			} else {
				errors.push("invalid hardwareConfig: " + words[2]);
			}
		} else if (words[0] == "waveguideLength") {
			waveguideLength = atof(words[2].c_str());
		} else if (words[0] == "maxFilterPressure") {
			maxPressure = atof(words[2].c_str());
		} else if (words[0] == "maxDepth") {
			maxDepth = atof(words[2].c_str());
		} else if (words[0] == "gpsCoordinates" || words[0] == "location") {
			location.fromString(words[2]);
		} else if (words[0] == "deploymentLabel") {
			deploymentLabel.assign(words[2]);
		} else if (words[0] == "powerSave") {
			powerSave = (words[2] == "1");
		} else if (words[0] == "portSwitching") {
			portSwitching = (words[2] == "1");
		} else if (words[0] == "ignoreFailures") {
			ignoreFailures = (words[2] == "1");
		} else if (words[0] == "logLevel") {
			vector<string> subwords(3);
			Util::split(words[2], 3, subwords);
			if (subwords.size() >= 1)
				logLevelConsole = logger.string2logLevel(subwords[0]);
			if (subwords.size() >= 2)
				logLevelStderr = logger.string2logLevel(subwords[1]);
			if (subwords.size() >= 3)
				logLevelDebug = logger.string2logLevel(subwords[2]);
		} else {
			errors.push("invalid line in config file: " + line);
			continue;
		}
	}
	ifs.close();
	doneReading = true;
	configString.assign(cfgStr);
	lck.unlock();
	// delay error messages until lock is released
	while (!errors.empty()) {
		logger.error(errors.front().c_str()); errors.pop();
	}
	return true;
}

} // ends namespace

