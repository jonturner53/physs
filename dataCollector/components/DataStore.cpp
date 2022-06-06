/** @file DataStore.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include <vector>
#include "DataStore.h"
#include "Spectrometer.h"
#include "Config.h"
#include "MaintLog.h"
#include "Status.h"
#include "LocationSensor.h"
#include "Spectrometer.h"
#include "SupplyPump.h"
#include "ScriptInterp.h"
#include "CollectorState.h"

namespace fizz {

extern string datapath;
extern string serialNumber;
extern Config config;
extern MaintLog maintLog;
extern Status hwStatus;
extern LocationSensor locationSensor;
extern Spectrometer spectrometer;
extern SupplyPump referencePump;
extern SupplyPump reagent1Pump;
extern SupplyPump reagent2Pump;
extern ScriptInterp scriptInterp;
extern CollectorState cstate;

/** Constructor for DataStore object.
 */
DataStore::DataStore() {
	deploymentIndex = 0;
	currentIndex = 0;
	spectrumCount = 0;
	openFlag = false;
	indexFlag = false;
}

/** Initialize state variables from CollectorState object.
 *  This should be called from the main thread, before starting other threads.
 */
void DataStore::initState() {
	cstate.getDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	indexFlag = true;
}

/** Open data file in which results are saved.
 *
 *  New results are appended to the end of the file,
 *  one record per line.
 */
bool DataStore::open() {
	unique_lock<mutex> lck(dataStoreMtx);
		return privateOpen();
}

/** Open data file in which results are saved.
 *  This version assumes the caller already holds the lock.
 *
 *  New results are appended to the end of the file,
 *  one record per line.
 */
bool DataStore::privateOpen() {
	if (openFlag) return true;
	string path = filePath(serialNumber, deploymentIndex);
	dataFile.open(path, ofstream::app);
	if (dataFile.fail()) {
		cerr << "DataStore: cannot open data file: " << path << "\n";
		return false;
	}
	openFlag = true;
	return true;
}

/** Path for file with specified serial number and deployment index.
 *  @param serialNumber is the serial number for the fizz
 *  @param depIndex is the deployment index for the data file
 */
string DataStore::filePath(const string& serialNumber, int depIndex) {
	char buf[20];
	snprintf(buf, sizeof(buf), "new%010d", depIndex);
	return datapath + "/sn" + serialNumber + "/raw/" + string(buf);
}

/** Close data file. */
void DataStore::close() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (openFlag) {
		dataFile.close();
		openFlag = false;
	}
}

/** Save a deployment record.
 *
 *  A new deployment record is added to the data file.
 *  Fields that can be inferred automatically are.
 */
void DataStore::saveDeploymentRecord() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!indexFlag) return;
	if (currentIndex < 1) {
		cerr << "DataStore: invalid current record index"
			 << currentIndex << endl;
		return;
	}
	deploymentIndex = currentIndex;
	recordMap.clear(); recordMap["dark"] = 1; // dummy entry
	spectrumCount = 0;

	if (openFlag) { dataFile.close(); openFlag = false; }
	if (!privateOpen()) return;

	string spectSerialNumber = spectrometer.getSerialNumber();
	vector<double> corrCoef = spectrometer.getCorrectionCoef();

	dataFile << "{ \"serialNumber\": " << serialNumber << ", "
		<< "\"index\": " << currentIndex << ", "
		<< "\"recordType\": \"deployment\", "
		<< "\"dateTime\": \"" << hwStatus.dateTimeString() << "\", "
		<< "\"label\": \"" << config.getDeploymentLabel() << "\", "
		<< "\"spectSerialNumber\": \"" << spectSerialNumber << "\", "
		<< "\"waveguideLength\": " << fixed << setprecision(4) 
		   << config.getWaveguideLength() << ", "
		<< "\"wavelengths\": [";
	char buf[20];
	for (int i = 0; i < Spectrometer::SPECTRUM_SIZE; i++) {
		snprintf(buf, 20, "%.2f", spectrometer.wavelengths[i]);
		dataFile << buf;
		if (i < Spectrometer::SPECTRUM_SIZE-1) dataFile << ", ";
	}
	dataFile << "], \"correctionCoef\": [" << scientific;
	for (unsigned int i = 0; i < corrCoef.size(); i++) {
		if (i > 0) dataFile << ", ";
		dataFile << corrCoef[i];
	}
	dataFile << "] }\n" << std::flush;

	currentIndex += 1;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

/** Save a cycle summary record.
 *
 *  A new cycle summary record is added to the data file.
 */
void DataStore::saveCycleSummary() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!privateOpen()) return;
	if (!indexFlag) return;

	char line[500];
	snprintf(line, sizeof(line),
		 "{ \"serialNumber\": %s, \"index\": %d, "
		 "\"recordType\": \"cycleSummary\", "
		 "\"dateTime\": \"%s\", \"deploymentIndex\": %d, "
		 "\"cycleNumber\": %ld, "
		 "\"temp\": %.1f, \"battery\": %.2f, "
		 "\"pressure\": %.2f, \"depth\": %.2f, \"location\": \"%s\", "
		 "\"integrationTime\": %.2f, "
		 "\"referenceLevel\": %.1f}",
		 serialNumber.c_str(), currentIndex,
		 hwStatus.dateTimeString().c_str(), deploymentIndex,
		 scriptInterp.getCycleNumber(),
		 hwStatus.temperature(), hwStatus.voltage(),
		 hwStatus.maxFilterPressure(), hwStatus.depth(),
		 locationSensor.getRecordedLocation().toString().c_str(),
		 spectrometer.getIntTime(), referencePump.getLevel());
	dataFile << line;
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		snprintf(line, sizeof(line),
			 "\"reagent1Level\": %.1f, \"reagent2Level\": %.1f, ",
			 reagent1Pump.getLevel(), reagent1Pump.getLevel());
		dataFile << line;
	}
	dataFile << endl << std::flush;

	currentIndex += 1;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

/** Save a spectrum record.
 *
 *  A new spectrum record is added to the data file.
 *  Fields that can be inferred automatically are.
 *
 *  @param spectrum vector of ints defining spectrum field.
 *  @param label label associated with this spectrum.
 *  @param prereq1label label of the first prerequisite spectrum
 *  			 (typically used for dark spectrum to be subtracted).
 *  @param prereq2label label of the second prerequisite spectrum
 *  			 (typically used for comparison spectrum).
 */
void DataStore::saveSpectrumRecord(vector<double>& spectrum,
					const string& label,
				   	const string& prereq1label,
				   	const string& prereq2label) {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!privateOpen()) return;
	if (!indexFlag) return;
	if (deploymentIndex == 0) {
		cerr << "DataStore: must save deployment record before "
				"spectra\n";
		return;
	}

	recordMap[label] = currentIndex;

	// find index of most recent spectrum matching prereq1label (if any)
	int prereq1index;
	if (prereq1label.length() == 0) {
		prereq1index = 0;
	} else {
		auto it = recordMap.find(prereq1label);
		if (it != recordMap.end()) {
			prereq1index = it->second;
		} else {
			cerr << "DataStore: saveSpectrumRecord: provided "
				"prereq1 label (" << prereq1label
				 << ") in record " << currentIndex
				 << " does not match any prior spectrum label\n";
			prereq1index = 0;
		}
	}
	
	// find index of most recent spectrum matching prereq2label (if any)
	int prereq2index;
	if (prereq2label.length() == 0) {
		prereq2index = 0;
	} else {
		auto it = recordMap.find(prereq2label);
		if (it != recordMap.end()) {
			prereq2index = it->second;
		} else {
			cerr << "DataStore: saveSpectrumRecord: provided "
				"prereq2 label (" << prereq1label << ") in "
				"record " << currentIndex << " does not match "
				"any prior spectrum label\n";
			prereq2index = 0;
		}
	}

	dataFile << "{ \"serialNumber\": " << serialNumber << ", "
		<< "\"index\": " << currentIndex << ", "
		<< "\"recordType\": \"spectrum\", "
		<< "\"dateTime\": \"" << hwStatus.dateTimeString() << "\", "
		<< "\"deploymentIndex\": " << deploymentIndex << ", "
		<< "\"prereq1index\": " << prereq1index << ", "
		<< "\"prereq2index\": " << prereq2index << ", "
		<< "\"label\": \"" << label << "\", \"spectrum\": [";
	char buf[20];
	for (int i = 0; i < Spectrometer::SPECTRUM_SIZE; i++) {
		snprintf(buf, 20, "%.2f", spectrum[i]);
		dataFile << buf;
		if (i < Spectrometer::SPECTRUM_SIZE-1) dataFile << ", ";
	}
	dataFile << "]}\n" << std::flush;

	currentIndex++; spectrumCount++;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

/** Encode a config string or script string or maintenance log string.
 *  Replaces double quote symbols with %% and newlines with @@.
 */
string DataStore::encodeConfigScript(string& s) {
	int i = 0; int j = s.find_first_of("\"\n", 0);
	string x;
	while (j >= 0) {
		x += s.substr(i, j-i);
		if (s[j] == '\"') x += "%%";
		else x += "@@";
		i = j+1;
		j = s.find_first_of("\"\n", i);
	}
	return x;
}

void DataStore::saveScriptRecord() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!privateOpen()) return;
	if (!indexFlag) return;

	string s = scriptInterp.getScriptString();

	char line[1000 + s.length()];
	snprintf(line, sizeof(line),
		 "{ \"serialNumber\": %s, \"index\": %d, "
		 "\"recordType\": \"script\", "
		 "\"dateTime\": \"%s\", \"deploymentIndex\": %d, "
		 "\"scriptString\": \"%s\"}\n",
		 serialNumber.c_str(), currentIndex, 
		 hwStatus.dateTimeString().c_str(), deploymentIndex,
		 encodeConfigScript(s).c_str());
	dataFile << line << std::flush;

	currentIndex += 1;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

void DataStore::saveConfigRecord() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!privateOpen()) return;
	if (!indexFlag) return;

	string s = config.getConfigString();

	char line[1000 + s.length()];
	snprintf(line, sizeof(line),
		 "{ \"serialNumber\": %s, \"index\": %d, "
		 "\"recordType\": \"config\", "
		 "\"dateTime\": \"%s\", \"deploymentIndex\": %d, "
		 "\"configString\": \"%s\"}\n",
		 serialNumber.c_str(), currentIndex, 
		 hwStatus.dateTimeString().c_str(), deploymentIndex,
		 encodeConfigScript(s).c_str());
	dataFile << line << std::flush;

	currentIndex += 1;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

void DataStore::saveMaintLogRecord() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!privateOpen()) return;
	if (!indexFlag) return;

	string s = maintLog.getMaintLogString();

	char line[1000 + s.length()];
	snprintf(line, sizeof(line),
		 "{ \"serialNumber\": %s, \"index\": %d, "
		 "\"recordType\": \"maintLog\", "
		 "\"dateTime\": \"%s\", \"deploymentIndex\": %d, "
		 "\"maintLogString\": \"%s\"}\n",
		 serialNumber.c_str(), currentIndex, 
		 hwStatus.dateTimeString().c_str(), deploymentIndex,
		 encodeConfigScript(s).c_str());
	dataFile << line << std::flush;

	currentIndex += 1;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

void DataStore::saveResetRecord() {
	unique_lock<mutex> lck(dataStoreMtx);
	if (!privateOpen()) return;
	if (!indexFlag) return;

	char line[300];
	snprintf(line, sizeof(line),
		 "{ \"serialNumber\": %s, \"index\": %d, "
		 "\"recordType\": \"reset\", "
		 "\"dateTime\": \"%s\", \"deploymentIndex\": %d}\n",
		 serialNumber.c_str(), currentIndex, 
		 hwStatus.dateTimeString().c_str(), deploymentIndex);
	dataFile << line << std::flush;

	currentIndex += 1;
	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

void DataStore::saveDebugRecord(const string& message) {
	// first save message in debugStrings
	// if main lock is not available, return and try again later
	// this is done to dodge deadlock situation
	unique_lock<mutex> dbsLock(dbsMtx);
	debugStrings.push(message);
	dbsLock.unlock();
	unique_lock<mutex> lck(dataStoreMtx, defer_lock);
	if (!lck.try_lock()) return;
	if (!privateOpen()) return;
	if (!indexFlag) return;

	while (true) {
		dbsLock.lock();
		if (debugStrings.empty()) {
			dbsLock.unlock(); break;
		}
		string s = debugStrings.front(); debugStrings.pop();
		dbsLock.unlock();

		string::size_type i = s.find('\n');
		if (i != string::npos) s.erase(i);
		dataFile << "{ "
				<< "\"serialNumber\": " << serialNumber << ", "
				<< "\"index\": " << currentIndex << ", "
				<< "\"recordType\": \"debug\", "
				<< "\"deploymentIndex\": " << deploymentIndex << ", "
				<< "\"message\": \"" << s << "\" }\n" << std::flush;
		currentIndex += 1;
	}

	cstate.setDataStoreState(currentIndex, deploymentIndex,
							 spectrumCount, recordMap);
	return;
}

} // ends namespace
