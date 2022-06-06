/** \file Config.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "stdinc.h" 
#include <atomic>
#include <mutex>
#include <vector>
#include "Coord.h"
#include "Util.h"
#include "SocketAddress.h"

using namespace std;

namespace fizz {

/** This class is used to read and update configuration variables.
 */
class Config {
public:		Config(const string&);
	
	bool	read();

	int		getHardwareConfig();
	string  getConfigString();
	Coord	getLocation();
	string	getDeploymentLabel();
	double	getWaveguideLength();
	double	getMaxPressure();
	double	getMaxDepth();
	int		getAutoRun();
	bool	getPowerSave();
	bool	getPortSwitching();
	bool	getIgnoreFailures();
	int		getLogLevel(const string&);

	enum	{ BASIC=101, TWO_REAGENTS=102 };
private:
	bool	doneReading;

	int	execMode;
	int	hardwareConfig;

	string	configString;
	string	configFile;
	Coord	location;
	string	deploymentLabel;
	double	waveguideLength;
	double	maxPressure;
	double	maxDepth;
	int		autoRun;
	bool	powerSave;
	bool	portSwitching;
	bool	ignoreFailures;
	int		logLevelConsole;
	int		logLevelStderr;
	int		logLevelDebug;

	mutex	cfgMtx;		///< used to sync method calls
};

inline int Config::getHardwareConfig() {
	unique_lock<mutex> lck(cfgMtx);
	return hardwareConfig;
}

inline string Config::getConfigString() {
	unique_lock<mutex> lck(cfgMtx);
	return configString;
}

inline Coord Config::getLocation() {
	unique_lock<mutex> lck(cfgMtx);
	return location;;
}

inline string Config::getDeploymentLabel() {
	unique_lock<mutex> lck(cfgMtx);
	return deploymentLabel;
}

inline double Config::getWaveguideLength() {
	unique_lock<mutex> lck(cfgMtx);
	return waveguideLength;
}
inline double Config::getMaxPressure() {
	unique_lock<mutex> lck(cfgMtx);
	return maxPressure;
}
inline double Config::getMaxDepth() {
	unique_lock<mutex> lck(cfgMtx);
	return maxDepth;
}
inline int Config::getAutoRun() {
	unique_lock<mutex> lck(cfgMtx);
	return autoRun;
}
inline bool Config::getPowerSave() {
	unique_lock<mutex> lck(cfgMtx);
	return powerSave;
}
inline bool Config::getPortSwitching() {
	unique_lock<mutex> lck(cfgMtx);
	return portSwitching;
}
inline bool Config::getIgnoreFailures() {
	unique_lock<mutex> lck(cfgMtx);
	return ignoreFailures;
}
inline int Config::getLogLevel(const string& s) {
	unique_lock<mutex> lck(cfgMtx);
	return (s == "console" ? logLevelConsole :
			(s == "stderr" ? logLevelStderr : logLevelDebug));
}

} // ends namespace

#endif
