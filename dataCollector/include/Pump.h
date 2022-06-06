/** \file Pump.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef PUMP_H
#define PUMP_H

#include <mutex>
#include "Logger.h" 

namespace fizz {

/** This class provides an api for controlling a pump.
 */
class Pump {
public:
			Pump(int, const string&, double=1);
	void	initState();

	double getMaxRate() {
		unique_lock<mutex> lck(puMtx);
		return maxRate;
	}
	double getCurrentRate() {
		unique_lock<mutex> lck(puMtx);
		return currentRate;
	}

	void	setMaxRate(double);

	virtual void on(double);
	void	off() { on(0); };

	string	getName() { return name; };
	int		getId() { return id; };

protected:
	double	maxRate;	///< maximum flow rate
	double	currentRate; ///< current flow rate
	string	name;		///< name of this pump

private:
	int		id;		///< numeric id (small positive int)
	mutex	puMtx;		///< mutex for individual pumps
	mutex	clMtx;		///< mutex for pump class to protect pumpSpeeds
};

} // ends namespace

#endif
