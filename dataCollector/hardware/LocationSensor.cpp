/** @file LocationSensor.cpp 
 *
 *  @author Jon Turner
 *  @date 2019
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "LocationSensor.h"
#include "Config.h"

namespace fizz {

extern Config config;

/** Constructor for a pressureSensor object.
 */
LocationSensor::LocationSensor() {
	recordedLocation = { 0.0, 0.0 };
}

/** Determine the current location.
 *  @return the gps coordinates of the current location
 */
Coord LocationSensor::read() {
	return config.getLocation();
}

} // ends namespace
