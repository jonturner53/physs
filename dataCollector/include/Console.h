/** \file Console.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include "stdinc.h" 
#include <mutex> 
#include "Util.h"
#include "SocketAddress.h"
#include "StreamSocket.h"
#include "Logger.h"
#include "LogTarget.h"

using namespace std;

namespace fizz {

/** This class provides an interface to a remote console.
 */
class Console : public LogTarget {
public:		Console();

	int		getLevel() { return LogTarget::getLevel(); };
	void	setLevel(int level) { LogTarget::setLevel(level); };

	bool	open(const string&, int);
	void	close();
	int		accept();
	bool	isConnected() { return connected; }

	int		readline(string&);
	void	reply(const string&);

	void	logMessage(const string&, int=Logger::MAXLEVEL);

private:
	SocketAddress serverAddr;
	SocketAddress clientAddr;
	StreamSocket serverSock;
	StreamSocket connSock;

	bool	connected;
	mutex	consoleMtx;
};

} // ends namespace

#endif
