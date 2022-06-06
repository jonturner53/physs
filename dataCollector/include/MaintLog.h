/** \file MaintLog.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef MAINTLOG_H
#define MAINTLOG_H

#include "stdinc.h" 
#include <atomic>
#include <mutex>
#include <vector>
#include "Coord.h"
#include "Util.h"
#include "SocketAddress.h"

using namespace std;

namespace fizz {

/** This class is used to read maintenance log, so it can be saved to
 *  data file when a new file is created.
 */
class MaintLog {
public:		MaintLog(const string&);
	
	bool	read();
	string  getMaintLogString();
private:
	string	maintLogString;
	string	maintLogFile;

	mutex	mlogMtx;		///< used to sync method calls
};

inline string MaintLog::getMaintLogString() {
	unique_lock<mutex> lck(mlogMtx);
	return maintLogString;
}

} // ends namespace

#endif
