/** \file LocationSensor.h
 *  @author Jon Turner
 *  @date 2019
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef LOCATIONSENSOR_H
#define LOCATIONSENSOR_H

#include <mutex>
#include <atomic>
#include <string>
#include "Coord.h"
#include "Config.h"

namespace fizz {

extern Config config;

/** This class provides an api for the location sensor.  */

class LocationSensor {
public:
		LocationSensor();
	void	recordLocation();
	Coord	getRecordedLocation();
private:
	Coord	recordedLocation;
	mutex	locMtx;

	Coord	read();
};

inline void LocationSensor::recordLocation() {
	unique_lock<mutex> lck(locMtx);
	recordedLocation = read();
}

inline Coord LocationSensor::getRecordedLocation() {
	unique_lock<mutex> lck(locMtx);
	return recordedLocation;
}


} // ends namespace

#endif
