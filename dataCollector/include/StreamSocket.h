/** \file StreamSocket.h
 *
 *  @author Jon Turner
 *  @date 2011
 *
 *  This software developed for Mote Marine Research Laboratory.
 */

#ifndef STREAM_SOCKET_H
#define STREAM_SOCKET_H

#include "stdinc.h"
#include "Util.h"
#include "Logger.h"
#include "SocketAddress.h"
#include "Socket.h"

using namespace std;

namespace fizz {

/** This class provides a convenient interface to stream socket.
 */
class StreamSocket : public Socket {
public:
		StreamSocket();
		~StreamSocket();

	bool	open();
	bool	open(SocketAddress&);
	void	close();
	int	accept(StreamSocket&, SocketAddress&);
	bool	connect(SocketAddress&);
	bool	getPeer(SocketAddress&);

	int	write(const string&);
	int	readline(string&, unsigned);

private:
	string	sbuf;		///< buffer holding characters not yet read
	int	sp;		///< characters in sbuf[0:sp] are not '\n'
};

} // ends namespace


#endif
