/** \file Socket.h
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software developed for Mote Marine Research Laboratory.
 */

#ifndef NP4D_H
#define NP4D_H

#include "stdinc.h"
#include "Util.h"
#include "SocketAddress.h"

using namespace std;

namespace fizz {

/** This class provides a more convenient API for socket programming
 *  and serves as a base class for the StreamSocket and DatagramSocket
 *  classes.
 */
class Socket {
public:
		Socket();
		~Socket();

	bool	open(const string&);
	bool	open(const string&, SocketAddress&);
	bool	getSocketAddress(SocketAddress&);
	bool	bind(SocketAddress&);
	bool	nonblock();

	// static utility methods
	static in_addr_t string2ip(const string&); 		
	static string ip2string(const in_addr_t&);		
	static bool getHostIp(const string&, string&);
	static bool getLocalIp(string&);

protected:
	int	sockNum;
};

} // ends namespace


#endif
