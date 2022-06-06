/** \file Interrupt.h
 *  @author Jon Turner
 *  @date 2021
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "stdinc.h" 
#include <mutex>
#include <condition_variable>
#include <vector>
#include "Util.h"
#include "Exceptions.h"
#include "Logger.h"

using namespace std;

namespace fizz {

struct InterruptClient {
	InterruptClient() {
		name = ""; handler = 0; active = urgent = detect = false;
	};
	InterruptClient(thread::id id, string name, void (*handler)(),
					bool active, bool urgent, bool detect)
					: id(id), name(name), handler(handler),
					  active(active), urgent(urgent), detect(detect) {
	}
	InterruptClient(const InterruptClient& c) {
		id = c.id; name = c.name; handler = c.handler;
		active = c.active; urgent = c.urgent; detect = c.detect;
	}

	thread::id  id;					// id of a client thread
	string  name;					// name of client (used in log messages)
	void	(*handler)();			// function called when interrupt detected
	bool    active;					// set when an interrupt requested
	bool	urgent;					// set for urgent requests
	bool	detect;					// set when interrupt detected by client
	condition_variable detected;	// requestor uses to wait for detection
	condition_variable cleared;		// client uses to wait for start/resume/end
};

extern Logger logger;

/** This class implements a Interrupt module. It allows one or more threads
 *  to be registered as "interruptible threads". Such a thread is expected
 *  to periodically check for pending interrupt requests, using the provided
 *  check or pause methds.
 */
class Interrupt {
public:	
	void	register_client(thread::id, string, void (*)());

	void	request(thread::id, bool=false);
	void	clear(thread::id);
	void	selfInterrupt();
	void	check();
	void	pause(double);
	bool	inProgress(thread::id);

private:
	thread::id id;		// id of the target thread
	string	name;		// name of the target thread

	bool	active;		// true when an interrupt is in progress
	bool	urgent;		// used to flag an urgent request
	bool	detect;		// set when target detects interrupt
	void	(*handler)(); // called when interrupt request detected

	vector<InterruptClient>  clientList;
	mutex	mtx;
};

} // ends namespace

#endif
