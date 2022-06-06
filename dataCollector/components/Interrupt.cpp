/** @file Interrupt.cpp 
 *
 *  @author Jon Turner
 *  @date 2021
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Util.h"
#include "Interrupt.h"

using this_thread::sleep_for;

namespace fizz {

/** Register a client thread with the interrupt object.
 *  @param id is the thread identifier for the client thread
 *  @param name is used to identify client in log messages
 *  @param handler is a pointer to a handler function that is
 *  called when an interrupt request is detected.
 *  @param id is the thread identifier for the client thread
 *  @param name is used to identify client in log messages
 */
void Interrupt::register_client(thread::id id, string name, void (*handler)()) {
    unique_lock<mutex> lck(mtx);
	logger.trace("Interrupt:: registering client %s", name.c_str());
    clientList.push_back(InterruptClient(id, name, handler, false,false,false));
}

/** Request an interrupt of target thread.
 *  @param urgent signifies an urgent interrupt request and
 *  this is typically used when shutting down a thread;
 *  if urgent is false, this method returns after the target detects
 *  the interrupt; the target then waits for the interrupt to be cleared;
 *  if urgent is true, this method returns immediately and the
 *  the target does not wait for the interrupt to clear.
 */
void Interrupt::request(thread::id id, bool urgent) {
	unique_lock<mutex> lck(mtx);
    for (InterruptClient& c : clientList) {
        if (id != c.id) continue;
		logger.trace("Interrupt:: got %srequest for %s",
					 (urgent ? "urgent " : ""), c.name.c_str());
		if (c.active && urgent) {
			c.active = false; c.detect = false; c.urgent = false;
            c.cleared.notify_one();
		} else if (!c.active) {
			c.active = true; c.urgent = urgent;
			if (!urgent)
				c.detected.wait(lck, [&]{return c.detect;});
		}
		return;
	}
}

/** Clear an interrupt, allowing target to continue; */
void Interrupt::clear(thread::id id) {
	unique_lock<mutex> lck(mtx);
    for (InterruptClient& c : clientList) {
        if (c.id != id) continue;
		logger.trace("Interrupt:: clearing interrupt for %s", c.name.c_str());
		c.active = false; c.detect = false; c.urgent = false;
		c.cleared.notify_one();
		return;
	}
}

/** Return true if an interrupt is in progress (including those that
 *  have been requested but not detected), else false.
 */
bool Interrupt::inProgress(thread::id id) {
	unique_lock<mutex> lck(mtx);
    for (InterruptClient& c : clientList) {
        if (c.id == id) return c.active;
	}
	return false;
}

/** Check to see if an interrupt request has been made for the current thread.
 *  If not, just return.  If so, run handler, then mark interrupt as detected
 *  to allow requestor to proceed. If the request is not urgent, wait for the
 *  interrupt to be cleared. Throw InterruptException when interrupt cleared.
 */
void Interrupt::check() {
	unique_lock<mutex> lck(mtx);
	thread::id id = this_thread::get_id();
    for (InterruptClient& c : clientList) {
        if (c.id != id) continue;
		if (!c.active) return;
		logger.trace("Interrupt:: %srequest detected for %s",
                     (c.urgent ? "urgent " : ""), c.name.c_str());
		c.handler();
		c.detect = true; c.detected.notify_one(); // allow requestor to proceed
		if (c.urgent) {
			c.active = c.detect = c.urgent = false; // implicit clear
		} else {
			c.cleared.wait(lck, [&]{ return !c.active; });  // wait for clear
		}
		logger.trace("interrupt cleared in %s", c.name.c_str());
		throw InterruptException();
	}
}

/** Initiate a self-interrupt for the calling thread.
 *  Run the handler. If there is already a pending urgent request, clear
 *  the interrupt and throw InterruptException. Otherwise, mark interrupt
 *  as detected (in case of a pending non-urgent request), wait for the
 *  the interrupt to be cleared, then throw InterrupException.
 */
void Interrupt::selfInterrupt() {
	unique_lock<mutex> lck(mtx);
	thread::id id = this_thread::get_id();
    for (InterruptClient& c : clientList) {
        if (c.id != id) continue;
		logger.trace("self-interrupt in %s", c.name.c_str());
		c.handler();
		if (c.active && c.urgent) {
			// we already have an urgent interrupt request
			c.active = false; c.detect = false; c.urgent = false;
		} else {
			c.active = true;
			c.detect = true; c.detected.notify_one(); 
				// in case of pending request
			c.cleared.wait(lck, [&]{ return !c.active; });  // wait for clear
		}
		logger.trace("interrupt cleared in %s", c.name.c_str());
		throw InterruptException();
	}
}

/** Delay for a specified amount of time, while checking for interrupts.
 *
 *  The check method is called every 50 ms, during long delay intervals.
 *  If it detects an interrupt request for the pending thread, it will
 *  handle it, eventually throwing an InterruptException when interrupt clears.
 *
 *  @param delay number of seconds to delay
 */
void Interrupt::pause(double delay) {
	double now = Util::elapsedTime();
	double stopTime = now + delay;
	while (now < stopTime) {
		check();
		double t0 = now;
		sleep_for(milliseconds(min(50, ((int) (1000 * (stopTime - now))))));
		now = Util::elapsedTime();
		if (now - t0 > .1)
			logger.error("Interrupt::pause: excessive delay %.3f s", now-t0);
	}
	check();
}

} // ends namespace
