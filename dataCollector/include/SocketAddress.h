/** \file SocketAddress.h
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software developed for Mote Marine Research Laboratory.
 */

#ifndef SOCKETADDRESS_H
#define SOCKETADDRESS_H

#include "stdinc.h"
#include "Util.h"

using namespace std;

namespace fizz {

/** Socket address provides more convenient interface to socket
 *  address structure.
 */
class SocketAddress {
public:
		SocketAddress();
		SocketAddress(const string&, int);
		~SocketAddress();

	SocketAddress& operator=(const SocketAddress&);
	string	getIp();
	int	getPort();
	string	toString();

	friend	class Socket;
	friend	class DatagramSocket;
	friend	class StreamSocket;
private:
	void	update(const sockaddr_in&);
	sockaddr_in sa;
        char*	ipCstring;
};

} // ends namespace

#endif
