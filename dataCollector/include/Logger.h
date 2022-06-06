/** \file Logger.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "stdinc.h" 
#include "Util.h"
#include "LogTarget.h"

using namespace std;

namespace fizz {

/** This class provides a simple message logging facility.
 *  Log targets are added using the addTarget method.
 */
class Logger {
public:		Logger();

	enum {	TRACE = 10, DEBUG = 20, DETAILS = 30, INFO = 40, 
			WARNING = 50, ERROR = 60, FATAL = 70, MAXLEVEL = 100
	};
	int		string2logLevel(string&);
	string	logLevel2string(int);

	void	trace(const char*, ...);
	void	debug(const char*, ...);
	void	details(const char*, ...);
	void	info(const char*, ...);
	void	warning(const char*, ...);
	void	error(const char*, ...);
	void	fatal(const char*, ...);
	void	log(char*, int, const string&);
	void	border(char='=');

	void	addTarget(LogTarget&);
private:
	vector<LogTarget*> targets;
};

inline int Logger::string2logLevel(string& s) {
	int level = 0;
	if 		(s == "trace") level = Logger::TRACE;
	else if (s == "debug") level = Logger::DEBUG;
	else if (s == "details") level = Logger::DETAILS;
	else if (s == "info") level = Logger::INFO;
	else if (s == "warning") level = Logger::WARNING;
	else if (s == "error") level = Logger::ERROR;
	else if (s == "fatal") level = Logger::FATAL;
	return level;
}

inline string Logger::logLevel2string(int level) {
	string s = "trace";
	if 		(level == Logger::TRACE) s = "trace";
	else if (level == Logger::DEBUG) s = "debug";
	else if (level == Logger::DETAILS) s = "details";
	else if (level == Logger::INFO) s = "info";
	else if (level == Logger::WARNING) s = "warning";
	else if (level == Logger::ERROR) s = "error";
	else if (level == Logger::FATAL) s = "fatal";
	return s;
}

} // ends namespace

#endif
