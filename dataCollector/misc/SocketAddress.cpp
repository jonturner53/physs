/** @file SocketAddress.cpp 
 *
 *  @author Jon Turner
 *  @date 2011
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#include "SocketAddress.h"

namespace fizz {

SocketAddress::SocketAddress() {
	ipCstring = 0;

	bzero(&sa, sizeof(sockaddr_in));
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = INADDR_ANY;
        sa.sin_port = 0;
}

SocketAddress::SocketAddress(const string& ipString, int port) {
	bzero(&sa, sizeof(sockaddr_in));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);

	if (ipString.length() == 0) {
		sa.sin_addr.s_addr = INADDR_ANY;
	} else {
		in_addr_t addr = inet_addr(ipString.c_str());
		if (addr == INADDR_NONE) {
			throw invalid_argument("bad IP address: " + ipString);
		}
		sa.sin_addr.s_addr = addr;
	}
	ipCstring = new char[20];
	strncpy(ipCstring, ipString.c_str(),19);

	if (port < 0 || port > 65535) {
		throw invalid_argument("bad port number: " + to_string(port));
	}
        sa.sin_port = htons(port);
}

SocketAddress::~SocketAddress() { 
	if (ipCstring != 0) delete [] ipCstring;
	ipCstring = 0;
}

SocketAddress& SocketAddress::operator=(const SocketAddress& x) {
	sa = x.sa;
	if (ipCstring != 0) delete [] ipCstring;
	ipCstring = 0;
	return *this;
}

/** Update the contents of a SocketAddress object.
 *  This is a private method intended for use by friend classes;
 *  it both updates the internal socket addess struct and clears the
 *  cached string representation of the address as needed.
 *  @param new_sa is a socket address struct specifying the new contents.
 */
void SocketAddress::update(const sockaddr_in& new_sa) {
	sa = new_sa;
	if (ipCstring != 0) delete [] ipCstring;
	ipCstring = 0;
}

string SocketAddress::getIp() {
	if (ipCstring == 0) {
		// initialize string from numeric address
		struct in_addr ipa;
		ipa.s_addr = sa.sin_addr.s_addr;

		ipCstring = new char[20];
		if (ipa.s_addr == 0) ipCstring[0] = '\0';
		else strncpy(ipCstring, inet_ntoa(ipa), 19);
	}
	return ipCstring;
}

int SocketAddress::getPort() { return ntohs(sa.sin_port); }

string SocketAddress::toString() {
	return "(" + getIp() + "," + to_string(getPort()) + ")";
}

} // ends namespace

