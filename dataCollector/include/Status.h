/** \file Status.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef STATUS_H
#define STATUS_H

#include <vector>
#include <mutex>
#include "Logger.h" 
#include "Config.h"

namespace fizz {

extern Logger logger;
extern Config config;

struct ppair { double offset; double scale; };
struct pdset { double cooked; int up; int down; };

/** This class provides an api for the various hardware status variables.  */
class Status {
public:
			Status();
	void	init();

	double	voltage();
	double	temperature();
	double	upstreamPressure();
	double	downstreamPressure();
	int		upstreamRawPressure();
	int		downstreamRawPressure();
	double	filterPressure();
	double	maxFilterPressure();
	double	depth();
	bool	leak();
	string	dateTimeString();

	bool	lowBattery();
	bool	tooHot();
	bool	overPressure();
	bool	tooDeep();

	void	clearMaxFilterPressure();
	void	recordDepth();

	bool	setPressure(double=-1);

	void	update();

private:
	// raw data values from arduino
	int		vbat;
	int		temp;
	int		upPressure;		
	int		downPressure;
	bool	leakStatus;
	string	dateTime;

	// offset and scaling parameters
	ppair	vbatParams;
	ppair	tempParams;
	ppair	upParams;
	ppair	downParams;

	vector<pdset> pressureData;

	double	maxPressureRecorded;	///< max filter pressure
	double  depthRecorded;

	mutex	statusMtx;
	double	cook(const int, const ppair&);
	int		raw(const double, const ppair&);
};

inline double Status::voltage(){
	unique_lock<mutex> lck(statusMtx);
	return cook(vbat, vbatParams);
}

inline double Status::temperature(){
	unique_lock<mutex> lck(statusMtx);
	return cook(temp, tempParams);
}

inline double Status::upstreamPressure(){
	unique_lock<mutex> lck(statusMtx);
	return cook(upPressure, upParams);
}

inline double Status::downstreamPressure(){
	unique_lock<mutex> lck(statusMtx);
	return cook(downPressure, downParams);
}

inline int Status::upstreamRawPressure(){
	unique_lock<mutex> lck(statusMtx);
	return upPressure;
}

inline int Status::downstreamRawPressure(){
	unique_lock<mutex> lck(statusMtx);
	return downPressure;
}

inline double Status::filterPressure(){
	unique_lock<mutex> lck(statusMtx);
	return cook(upPressure, upParams) - cook(downPressure, downParams);
}

inline double Status::maxFilterPressure(){
	unique_lock<mutex> lck(statusMtx);
	return maxPressureRecorded;
}

inline double Status::depth(){
	unique_lock<mutex> lck(statusMtx);
	return depthRecorded;
}

inline bool Status::leak() {
	unique_lock<mutex> lck(statusMtx);
	return leakStatus;
}

inline string Status::dateTimeString() {
	unique_lock<mutex> lck(statusMtx);
	return dateTime;
}

inline bool Status::lowBattery() {
	return (voltage() < 10.);
}

inline bool Status::tooHot() {
	return (temperature() > 60.);
}

/** Determine if the pressure across filter is excessive.
 *  @return true if pressure across filter exceeds specified limit.
 */
inline bool Status::overPressure() {
	return (filterPressure() > config.getMaxPressure());
}

/** Determine if the depth is excessive.
 *  @return true if depth exceeds the maximum safe depth.
 */
inline bool Status::tooDeep() {
	return (depth() > config.getMaxDepth());
}

inline void	Status::clearMaxFilterPressure() {
	unique_lock<mutex> lck(statusMtx);
	maxPressureRecorded = 0.;
}

inline void Status::recordDepth() {
	unique_lock<mutex> lck(statusMtx);
	depthRecorded = .685 * cook(downPressure, downParams);
}

/** Convert a raw value to a cooked value. */
inline double Status::cook(const int v, const ppair& params) {
	return max(0.0, v - params.offset) / max(1.0, params.scale);
}

/** Convert a cooked value to a raw value. */
inline int Status::raw(const double v, const ppair& params) {
	return (v * params.scale) + params.offset;
}

} // ends namespace

#endif
