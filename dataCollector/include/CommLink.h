/** \file CommLink.h
 *  @author Jon Turner
 *  @date 2018
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef COMMLINK_H
#define COMMLINK_H

#include <atomic>
#include "Logger.h" 
#include "Config.h"
#include "Arduino.h"

using std::this_thread::sleep_for;

namespace fizz {

extern Config config;
extern Arduino arduino;

/** This class provides an api for controlling the communications link
 *  connecting the physs to a remote cloud server. This includes both
 *  power to the comm device (typically a cell phone modem) plus the
 *  ssh connection and tunnels.
 */
class CommLink {
public:
		CommLink() {
			enabled.store(true);
		};

	bool	state() { return enabled.load(); };
	bool	isActive();
	void	enable();
	void	disable();
private:
	atomic<bool> enabled;	///< true when commLink turned on
};

inline void CommLink::enable() {
	arduino.send("M1");
	system("systemctl start fizzTunnels.service");
	enabled.store(true);
}

inline void CommLink::disable() {
	enabled.store(false);
	system("systemctl stop fizzTunnels.service");
	arduino.send("M0");
}

inline bool CommLink::isActive() {
	/* run command to determine status of ssh connection to coolcloud */
	FILE *fp;
	fp = popen("systemctl show -p ActiveState fizzTunnels.service", "r");
	if (fp == NULL) {
		// if we can't determine state, disable it and return false
		disable(); return false;
	}
	/* read command output */
	char line[256];
	if (fgets(line, sizeof(line)-1, fp) == NULL) {
		// if we can't determine state, disable it and return false
		disable(); return false;
	}
	pclose(fp);
	return (strstr(line, "ActiveState=active") == line);
}

} // ends namespace

#endif
