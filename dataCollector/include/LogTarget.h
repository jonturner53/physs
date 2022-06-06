/** \file LogTarget.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef LOGTARGET_H
#define LOGTARGET_H

#include <atomic>

using namespace std;

namespace fizz {

/** This is a virtual class which serves as a base class for
 *  any class that receives log messages. A LogTarget must
 *  implement the logMessage method.
 */
class LogTarget {
public:		LogTarget() { logLevel.store(0); };
			LogTarget(int level) { logLevel.store(level); };

	int		getLevel() { return logLevel.load(); }
	void	setLevel(int level) { logLevel = level; }
	virtual void logMessage(const string&, int) {} ;
protected:
	atomic_int logLevel;
};

} // ends namespace

#endif
