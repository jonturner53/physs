/** @file MaintLog.cpp 
 *
 *  @author Jon Turner
 *  @date 2017

 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "MaintLog.h"
#include "Logger.h"

namespace fizz {

extern Logger logger;

/** Constructor for MaintLog objects.
 *  @param maintLogFile name of the maintLog file
 */
MaintLog::MaintLog(const string& maintLogFile) : maintLogFile(maintLogFile) {
	maintLogString = "";
}

/** Read maintLog file.
 */
bool MaintLog::read() {
	logger.debug("MaintLog: reading maintLog file");
	unique_lock<mutex> lck(mlogMtx);
	ifstream ifs;
	ifs.open(maintLogFile, ifstream::in);
	if (ifs.fail()) {
		lck.unlock();
		logger.error("cannot open maintLog file\n");
		return false;
	}
	string mlogString;
	while (!ifs.eof()) {
		char buf[1024]; buf[sizeof(buf)-1] = '\0';
		ifs.getline(buf, sizeof(buf));   // discards newline
		string line = buf;
		mlogString += line + '\n';
	}
	ifs.close();
	maintLogString.assign(mlogString);
	lck.unlock();
	return true;
}

} // ends namespace

