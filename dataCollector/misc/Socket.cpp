/** @file Socket.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#include "Socket.h"

namespace fizz {

/** Constructor for Socket objects. */
Socket::Socket() { sockNum = 0; }

/** Destructor for Socket objects. */
Socket::~Socket() { if (sockNum != 0) close(sockNum); }

/** Open the socket.
 *  @param sockType is one of the strings "stream" or "datagram".
 *  @return true on success, false on failure
 */
bool Socket::open(const string& sockType) {
	int typeCode;

	if (sockType.compare("stream") == 0) typeCode = SOCK_STREAM;
	else if (sockType.compare("datagram") == 0) typeCode = SOCK_DGRAM;
	else return false;

	sockNum = socket(AF_INET, typeCode , 0);
	return (sockNum != 0 ? true : false);
}

/** Open the socket and bind it to a specified socket address.
 *  @param sockType is one of the strings "stream" or "datagram".
 *  @param sockAddr is an appropriately initialized SocketAddress object.
 *  @return true on success, false on failure
 */
bool Socket::open(const string& sockType, SocketAddress& sockAddr) {
	if (!open(sockType)) return false;
        return bind(sockAddr);
}

/** Bind a socket to a specified SocketAddress.
 *  @param sockAddr is an appropriately initialized SocketAddress object
 *  @return true on success, false on failure
 */
bool Socket::bind(SocketAddress& sockAddr) {
	int x = ::bind(sockNum,(struct sockaddr *) &sockAddr.sa,
		       sizeof(sockAddr.sa));
	return (x == 0);
}

/** Get the socket address associated with the socket.
 *  @param sockAddr is a SocketAddress object in which result is returned
 *  @return true on success, false on failure
 */
bool Socket::getSocketAddress(SocketAddress& sockAddr) {
	sockaddr_in sa; socklen_t len = sizeof(sa); bzero(&sa, len);
	if (getsockname(sockNum, (struct sockaddr *) &sa, &len) < 0)
		return false;
	sockAddr.update(sa);
	return true;
}


/** Configure the socket to be nonblocking (must be open already).
 *  @return true on success, false on failure
 */
bool Socket::nonblock() {
        int flags;
        if ((flags = fcntl(sockNum, F_GETFL, 0)) < 0) return false;
        flags |= O_NONBLOCK;
        if ((flags = fcntl(sockNum, F_SETFL, flags)) < 0) return false;
        return true;
}

/** Convert a string to an IP address.
 *  @param ipString is an IPv4 address in dotted decimal notation
 *  @return the IPv4 address associated with the given string in host
 *  byte order, or 0 if string does not represent a valid IPv4 address
 */
in_addr_t Socket::string2ip(const string& ipString) {
	in_addr_t ipa = inet_addr(ipString.c_str());
	if (ipa == INADDR_NONE) return 0;
	return ntohl(ipa);
}

/** Create a string representation of an IP address.
 *  @param ipa is the IP address in host byte order
 *  @return the string representing ipa
 */
string Socket::ip2string(const in_addr_t& ipa) {
	struct in_addr ipa_struct;
	ipa_struct.s_addr = htonl(ipa);
	string s = inet_ntoa(ipa_struct);
	return s;
}

/** Get the default IP address of a specified host
 *  @param hostName is the name of the host
 *  @param hostIP is a string in which the host's IP address is returned
 *  @return true on success, false on failure
 */
bool Socket::getHostIp(const string& hostName, string& hostIp) {
        hostent* host = gethostbyname(hostName.c_str());
        if (host == NULL) return false;
	hostIp = ip2string(ntohl(*((in_addr_t*) &host->h_addr_list[0][0])));
	return true;
}

/** Get the default IP address of this host.
 *  @param localIp is a string in which the local IP address is returned
 *  @return true on success, false on failure
 */
bool Socket::getLocalIp(string& localIp) {
 	char myName[1001];
        if (gethostname(myName, 1000) != 0) return false;
	return getHostIp(myName, localIp);
}

} // ends namespace

