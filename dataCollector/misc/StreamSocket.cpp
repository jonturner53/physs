/** @file StreamSocket.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "StreamSocket.h"

namespace fizz {

StreamSocket::StreamSocket() {
	sp = 0;
}

StreamSocket::~StreamSocket() {}

/** Open a socket for a client endpoint.
 *  return true on success, false on failure.
 */
bool StreamSocket::open() { return Socket::open("stream"); }

/** Open a socket for a server endpoint.
 *  @param sockAddr is a SocketAddress object specifying the socket address
 *  used for listening
 *  return true if the socket is successfully created, bound to the specified
 *  endpoint and placed in the listening state, else return false.
 */
bool StreamSocket::open(SocketAddress& sockAddr) {
	if (!Socket::open("stream", sockAddr)) return false;
	return (listen(sockNum, 10) == 0);
}

void StreamSocket::close() { ::close(sockNum); }

/** Get the socket address of this socket's peer.
 *  @param peerAddr is a SocketAddress object in which the result is returned
 *  @return the address of the peer, or 0 on failure
 */
bool StreamSocket::getPeer(SocketAddress& peerAddr) {
	sockaddr_in sa; socklen_t len = sizeof(sa); bzero(&sa, len);
	if (getpeername(sockNum, (struct sockaddr *) &sa, &len) < 0)
		return false;
	peerAddr.update(sa);
	return true;
}

/** Accept the next waiting connection request and get the peer socket address.
 *  @param connSock is a StreamSocket object which on successful return,
 *  represents the new connection socket.
 *  @param peer is a SocketAddress object in which the peer socket address
 *  is returned.
 *  @return -1 if no incoming connection available (applies to nonblocking
 *  connections), -2 if an error occurred; otherwise the socket number of
 *  the new connection.
 */
int StreamSocket::accept(StreamSocket& connSock, SocketAddress& peer) {
	sockaddr_in sa; socklen_t len = sizeof(sa); bzero(&sa, len);
	int connSockNum = ::accept(sockNum, (struct sockaddr *) &sa, &len);
	if (connSockNum < 0)
		return (errno == EAGAIN || errno == EWOULDBLOCK) ? -1 : -2;
	peer.update(sa);
	connSock.sockNum = connSockNum;
	return connSockNum;
}

/** Connect to a remote host.
 *  @param serverAddr is a SocketAddress object for the remote host
 *  @return true on success, false on failure
 */
bool StreamSocket::connect(SocketAddress& serverAddr) {
	sockaddr_in sa; socklen_t len = sizeof(sa); bzero(&sa, len);
	if (::connect(sockNum, (struct sockaddr *) &sa, len) != 0)
		return false;
	serverAddr.update(sa);
	return true;
}

/** Write a string to a stream socket.
 *  @param s is the string to be sent
 *  @param return an integer whose magnitude is equal to the number of
 *  characters written; this will usually be equal to the length of s,
 *  but for nonblocking sockets it may be less than the length of s
 *  if the socket buffer is full; if an error occurs the returned value
 *  will be negative, but its magnitude will be the number of characters
 *  written.
 */
int StreamSocket::write(const string& s) {
	const char *p = s.c_str();
	int numLeft = s.length();
	while (numLeft > 0) {
		int n = send(sockNum, (void *) p, numLeft, 0);
		if (n < 0) {
			return (errno == EAGAIN || errno == EWOULDBLOCK) ?
				s.length() - numLeft : -(s.length() - numLeft);
		}
		if (n == 0) return s.length() - numLeft;
		numLeft -= n; p += n;
	}
	return s.length();
}

/** Read a line from the socket.
 *  @param s is a string object in which result is returned
 *  (the newline is not included in s); if no complete line is
 *  available, the returned string is empty (this case can arise if the
 *  socket is closed, there is an error or if the socket is nonblocking).
 *  @param maxLength is the maximum line length allowed; excessively long
 *  lines are silently deleted;
 *  @return 0 if connection has been closed by peer, -1 if no data is
 *  available on socket (for nonblocking sockets), -2 if an error occurs
 *  on the socket; otherwise the number of bytes returned.
 */
int StreamSocket::readline(string& s, unsigned maxLength) {
	char cbuf[16384]; bool toolong;

	s.erase(0); toolong = false;
	while (true) {
		unsigned long i = sbuf.find('\n', sp);
		if (i != string::npos) {
			if (not toolong) {
				s.assign(sbuf, 0, i);
				sbuf.erase(0, i+1); sp = 0;
				return s.length(); // return complete line
			}
			// erase remainder of excessively long line
			sbuf.erase(0, i+1); sp = 0; toolong = false;
			continue;
		} else if (sbuf.length() >= maxLength) {
			// silently discard bytes from current line
			sbuf.erase(); sp = 0; toolong = true;
		}
		sp = sbuf.length();
		int n = recv(sockNum, cbuf, sizeof(cbuf), 0);
		if (n == 0) { // peer has closed socket
			s.assign(sbuf); sbuf.erase();
			return s.length();
		} else if (n < 0) {
			return (errno == EAGAIN || errno == EWOULDBLOCK) ?
				-1 : -2;
		}
		sbuf.append(cbuf, n);
	}
}

} // ends namespace

