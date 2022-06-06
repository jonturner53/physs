/** \file Arduino.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef ARDUINO_H
#define ARDUINO_H

#include "stdinc.h" 
#include <atomic>
#include <mutex>
#include <vector>

using namespace std;

namespace fizz {

/** This class provides an api for communicating with an arduino
 */
class Arduino {
public:		Arduino();
			~Arduino();

	bool	start();
	void	finish();

	string	command(const string&, bool=false);
	void	send(const string& s) { command(s); }
	string	query(const string& s, bool force=false) {
		return command(s, force);
	}
	void	log();
	bool	isReady() { return ready.load(); };
	bool	isEquipped() { return equipped.load(); };

	int		stressTest(int=1000, double=.05);
private:
	atomic<bool> ready;		///< true when arduino communicating
	atomic<bool> equipped;	///< true if arduino has control board

	int	fd;		///< file descriptor for serial link
	string	buf;
	bool	quit;
	thread	readerThread;

	mutex	ardMtx;		///< mutual exclusion for send and query
	mutex	bufMtx;		///< mutual exclusion for buf string

	bool	setupSerialLink(int);
	void	reader();
	static void startReader(Arduino&);

	int		failureCount;
};

} // ends namespace

#endif
