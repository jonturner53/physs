/** \file CollectorState.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef COLLECTOR_STATE_H
#define COLLECTOR_STATE_H

#include "stdinc.h" 
#include <vector>
#include <unordered_map>
#include <mutex>
#include "Util.h"

using namespace std;

namespace fizz {

/** This class holds shadow copies of state variables owned
 *  by several "client methods" and provides methods to update
 *  the shadow variables. Whenever a variable is updated, the
 *  values of all shadow variables are written to an external
 *  state file.
 */
class CollectorState {
public:		CollectorState(const string&);
	
	bool	read();

	int 	getCycleNumber();
	double	getMaxRate(const string&);
	double	getSupplyLevel(const string&);
	double	getPressureSensor(const string&);
	double	getIntegrationTime();
	void	getDataStoreState(int&, int&, int&, unordered_map<string,int>&);

	void 	setCycleNumber(int);
	void	setMaxRate(const string&, double);
	void	setSupplyLevel(const string&, double);
	void	setPressureSensor(const string&, double);
	void	setIntegrationTime(double);
	void	setDataStoreState(int, int, int, unordered_map<string,int>&);

private:
	bool	doneReading;
	string	stateFile;
	mutex	cstateMtx;

	int 	cycleNumber;

	double	samplePumpMaxRate;
	double	referencePumpMaxRate;
	double	reagent1PumpMaxRate;
	double	reagent2PumpMaxRate;

	double	referenceSupplyLevel;
	double	reagent1SupplyLevel;
	double	reagent2SupplyLevel;

	double	pressureSensorUpstreamOffset;
	double	pressureSensorUpstreamScale;
	double	pressureSensorDownstreamOffset;
	double	pressureSensorDownstreamScale;

	double	integrationTime;

	int		currentIndex;
	int		deploymentIndex;
	int		spectrumCount;
	unordered_map<string,int> recordMap;

	int	get(int*);
	double	get(double*);
	void	set(int*, int);
	void	set(double*, double);
	bool	update();
};

inline int CollectorState::get(int* p) {
	unique_lock<mutex> lck(cstateMtx);
	if (doneReading) return(*p);
	cerr << "CollectorState:: attempting to access "
		"variable before state file is read\n";
	exit(1);
}

inline double CollectorState::get(double* p) {
	unique_lock<mutex> lck(cstateMtx);
	if (doneReading) return(*p);
	cerr << "CollectorState:: attempting to access "
		"variable before state file is read\n";
	exit(1);
}

inline void CollectorState::set(int* p, int v) {
	unique_lock<mutex> lck(cstateMtx);
	if (doneReading) { *p = v; update(); return; }
	cerr << "CollectorState:: attempting to set state "
		"variable before state file is read\n";
	exit(1);
}

inline void CollectorState::set(double* p, double v) {
	unique_lock<mutex> lck(cstateMtx); *p = v;
	if (doneReading) { *p = v; update(); return; }
	cerr << "CollectorState:: attempting to set state "
		"variable before state file is read\n";
	exit(1);
}

inline int CollectorState::getCycleNumber() { return get(&cycleNumber); }

inline double CollectorState::getMaxRate(const string& name) {
	if (name == "samplePump")
		return get(&samplePumpMaxRate);
	else if (name == "referencePump")
		return get(&referencePumpMaxRate);
	else if (name == "reagent1Pump")
		return get(&reagent1PumpMaxRate);
	else if (name == "reagent2Pump")
		return get(&reagent2PumpMaxRate);
	return 0; // should never get here
}

inline double CollectorState::getSupplyLevel(const string& name) {
	if (name == "referencePump")
		return get(&referenceSupplyLevel);
	else if (name == "reagent1Pump")
		return get(&reagent1SupplyLevel);
	else if (name == "reagent2Pump")
		return get(&reagent2SupplyLevel);
	return 0; // should never get here
}

inline double CollectorState::getPressureSensor(const string& prop) {
	if (prop == "upstreamOffset")
		return get(&pressureSensorUpstreamOffset);
	else if (prop == "upstreamScale")
		return get(&pressureSensorUpstreamScale);
	else if (prop == "downstreamOffset")
		return get(&pressureSensorDownstreamOffset);
	else if (prop == "downstreamScale")
		return get(&pressureSensorDownstreamScale);
	return 0; // should never get here
}

inline double CollectorState::getIntegrationTime() {
	return get(&integrationTime);
}

inline void CollectorState::getDataStoreState(
	int& x, int& d, int& sc, unordered_map<string,int>& m) {
	unique_lock<mutex> lck(cstateMtx);
	if (doneReading) {
		x = currentIndex; d = deploymentIndex;
		sc = spectrumCount; m = recordMap;
		return;
	}
	cerr << "CollectorState:: attempting to access "
		"data store state before state file is read\n";
	exit(1);
}

inline void  CollectorState::setCycleNumber(int c) {
	return set(&cycleNumber, c);
}

inline void CollectorState::setMaxRate(const string& name, double rate) {
	if (name == "samplePump")
		return set(&samplePumpMaxRate, rate);
	else if (name == "referencePump")
		return set(&referencePumpMaxRate, rate);
	else if (name == "reagent1Pump")
		return set(&reagent1PumpMaxRate, rate);
	else if (name == "reagent2Pump")
		return set(&reagent2PumpMaxRate, rate);
}

inline void CollectorState::setSupplyLevel(const string& name, double level) {
	if (name == "referencePump")
		return set(&referenceSupplyLevel, level);
	else if (name == "reagent1Pump")
		return set(&reagent1SupplyLevel, level);
	else if (name == "reagent2Pump")
		return set(&reagent2SupplyLevel, level);
}

inline void CollectorState::setPressureSensor(const string& prop,
					      double value) {
	if (prop == "upstreamOffset")
		return set(&pressureSensorUpstreamOffset, value);
	else if (prop == "upstreamScale")
		return set(&pressureSensorUpstreamScale, value);
	else if (prop == "downstreamOffset")
		return set(&pressureSensorDownstreamOffset, value);
	else if (prop == "downstreamScale")
		return set(&pressureSensorDownstreamScale, value);
}

inline void CollectorState::setIntegrationTime(double t) {
	return set(&integrationTime, t);
}

inline void CollectorState::setDataStoreState(
	int x, int d, int sc, unordered_map<string,int>& m) {
	unique_lock<mutex> lck(cstateMtx);
	if (doneReading) {
		currentIndex = x; deploymentIndex = d; 
		spectrumCount = sc; recordMap = m;
		update();
		return;
	}
	cerr << "CollectorState:: attempting to set "
		"data store state before state file is read\n";
	exit(1);
}

} // ends namespace

#endif
