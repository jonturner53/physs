/** \file Console.cpp;
 *  @author Jon Turner;
 *;
 *  This software was developed for Mote Marine Research Laboratory.;
 */;

#include "Console.h"

using namespace std;

namespace fizz {

/** Constructor for Console object.
 *  @param logLevel is the logging level used for reporting log messages.
 */
Console::Console() {
	this->serverAddr = serverAddr;
	connected = false;
}

/** Open server socket.
 *  @param ipAddress is the address to be used for the listening socket.
 *  @param portNum is the port number to be used.
 *  @return true on success, false on failure.
 */
bool Console::open(const string& ipAddress, int portNum) {
	unique_lock<mutex> consoleLock(consoleMtx);

	serverAddr = SocketAddress(ipAddress, portNum);
	if (!serverSock.open(serverAddr) || !serverSock.nonblock()) {
		cerr << "Console::open: cannot open/configure socket "
		     << "at address (" << ipAddress << "," << to_string(portNum)
		     << ")\n";
		return false;
	}
	return true;
}

/** Close the console socket. */
void Console::close() {
	unique_lock<mutex> consoleLock(consoleMtx);
	connSock.close();
	connected = false;
}

/** Check for connection from remote console and if incoming request
 *  has come in, accept it.
 *  @return -1 if no incoming connection request is available, -2 if there
 *  is an error on the socket; otherwise, return 1.
 */
int Console::accept() {
	unique_lock<mutex> consoleLock(consoleMtx);
	if (connected) {
		cerr << "Console:accept: accept invalid when connection "
			"already active\n";
		return -2;
	}
	int status = serverSock.accept(connSock, clientAddr);
	if (status < 0) return status;
	if (!connSock.nonblock()) return -2;
	connected = true;
	cerr << "Console: accepted connection from "
	     << clientAddr.toString() << endl;
	return 1;
}

/** Read a line from the socket (if connected).
 *  @param line is a string in which line of input is returned.
 *  @return 0 if connection has been closed by peer, -1 connection remains
 *  open but no complete line is available yet, -2 if an error has occurred
 *  on the socket; otherwise return the length of the line returned.
 */
int Console::readline(string& line) {
	unique_lock<mutex> consoleLock(consoleMtx);
	if (!connected) return -2;
	int status = connSock.readline(line, 1024);
		// long lines are discarded to block buffer-overflow attack
	if (status == 0 || status == -2) connected = false;
	return status;
}

/** Write a log message to the console.;
 *  @param s is message to be written
 *  @param level is the log level for this method; the message should
 *  be written if level exceeds the threshold for this LogTarget.
 */
void Console::logMessage(const string& s, int level) {
	unique_lock<mutex> consoleLock(consoleMtx);
	if (!connected || level < this->logLevel) return;
	if (connSock.write(s) < 0) {
		connected = false;
	}
}

/** Send a reply to the console.;
 *  @param s is string to be written
 */
void Console::reply(const string& s) {
	unique_lock<mutex> consoleLock(consoleMtx);
	if (connected) {
		if (connSock.write("|" + s + "\n") < 0) {
			connected = false;
		}
	} else {
		cout << s << endl;
			// this is mostly useful when first starting up,;
			// allows messages to reach user before console has;
			// been connected;
	}
}

} // ends namespace
