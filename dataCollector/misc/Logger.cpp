/** @file Logger.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#include "Logger.h"

namespace fizz {

Logger::Logger() {
	Util::elapsedTime(); // ensures initialization
}

/** Add a logging target. Only used by constructors. */
void Logger::addTarget(LogTarget& target) {
	targets.push_back(&target);
}

void Logger::trace(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, TRACE, "TRACE");
}

void Logger::debug(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, DEBUG, "DEBUG");
}

void Logger::details(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, DETAILS, "DETAILS");
}

void Logger::info(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, INFO, "INFO");
}

void Logger::warning(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, WARNING, "WARNING");
}

void Logger::error(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, ERROR, "ERROR");
}

void Logger::fatal(const char *format, ...) {
	char buf[500];
	va_list args;
	va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf, FATAL, "FATAL");
}

void Logger::log(char *buf, int level, const string& levelName) {
	double t = Util::elapsedTime();
	
	char tbuf[20];
	snprintf(tbuf, sizeof(tbuf), "%.3f", t);

	string s = string(buf) + " [" + levelName + " " + string(tbuf) + "]\n";
	for (unsigned int i = 0; i < targets.size(); i++) {
		targets[i]->logMessage(s, level);
	}
}

void Logger::border(char c) {

	string s(50, c); s += "\n";
	for (unsigned int i = 0; i < targets.size(); i++) {
		targets[i]->logMessage(s.c_str(), FATAL);
	}
}


} // ends namespace
