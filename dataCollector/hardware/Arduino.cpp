/** \file Arduino.cpp
 *  @author Jon Turner
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Arduino.h"
#include "Util.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "Logger.h"

using namespace chrono;
using this_thread::sleep_for;

namespace fizz {

extern Logger logger;

Arduino::Arduino() {
	quit = false;
	ready.store(false);
	equipped.store(false);
	failureCount = 0;
}

Arduino::~Arduino() {}

bool Arduino::setupSerialLink(int fd) {
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		printf("Error from tcgetattr: %s\n", strerror(errno));
		return -1;
	}

	cfsetospeed(&tty, B115200);
	cfsetispeed(&tty, B115200);

	tty.c_cflag |= (CLOCAL | CREAD);	/* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;		 /* 8-bit characters */
	tty.c_cflag &= ~PARENB;	 /* no parity bit */
	tty.c_cflag &= ~CSTOPB;	 /* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS;	/* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
			 INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* wait up to a second for data */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 10;

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		logger.error("Arduino: serial setup error: %s\n", strerror(errno));
		return false;
	}
	return true;
}

bool Arduino::start() {
	for (int i = 0; i < 10; i++) {
		char fnam[50];
		snprintf(fnam, sizeof(fnam)-1, "/dev/ttyUSB%d", i);
		fd = open(fnam, O_RDWR | O_NOCTTY);
		if (fd >= 0) break;
		if (i == 9) {
			logger.debug("Arduino: unable to open serial link");
			return false;
		}
	}
	if (!setupSerialLink(fd)) {
		logger.debug("Arduino: unable to configure serial link");
		return false;
	}
	sleep_for(seconds(2));

	quit = false;
	readerThread = thread(&Arduino::startReader, ref(*this));
	sleep_for(seconds(1)); // allow reader to run

	failureCount = 0;
	string s = command("ehello", true);
	if (s != "hello") {
		logger.debug("Arduino: unable to communicate with arduino");
		return false;
	}
	ready.store(true);

	s = command("H");
	if (s == "1") equipped.store(true);

	logger.debug("Arduino: arduino is active and %s equipped",
				(s == "1" ? "is" : "not"));

	return true;
}

void Arduino::startReader(Arduino& ard) { ard.reader(); }

void Arduino::finish() {
	unique_lock<mutex> lck(ardMtx);
	quit = true;
	if (readerThread.joinable()) readerThread.join();
	close(fd);
	logger.debug("Arduino: closed connection");
}

void Arduino::reader() {
	char cbuf[1024];

	while (true) {
		int n = read(fd, cbuf, sizeof(cbuf)-1);
		if (quit) break;
		if (n == 0) continue;
		if (n < 0) {
			logger.debug("Arduino: read error: %s\n", strerror(errno));
		} else {
			cbuf[n] = 0;
			bufMtx.lock(); buf += cbuf; bufMtx.unlock();
		}
	}
}

string Arduino::command(const string& s, bool force) {
	if (!force && !isReady()) return "";
	unique_lock<mutex> lck(ardMtx);

	// clear buffer before sending command
	bufMtx.lock(); buf.clear(); bufMtx.unlock();

	string reply; unsigned int i, j;
	i = 0; // all characters up to buf[i-1] have been examined
	for (int k = 0; k < 25; k++) {
		if (k == 0 || k == 8 || k == 16) {
			// retry up to 2 times
			if (write(fd, (s + ".\n").c_str(), s.length() + 2) < 0) {
				logger.debug("arduino write failure, disabling");
				ready.store(false);
				return "";
			}
		}
		sleep_for(milliseconds(5));
		bufMtx.lock();
		if (i >= buf.length()) {
			bufMtx.unlock(); continue;
		}
		// examine new characters
		j = i;
		while (j < buf.length() && buf[j] != '\n') j++;
		if (j < buf.length()) {
			// buf[j] == '\n'

			// complicated for backward compatibility
			if (j > 0 && (buf[j-1] == '.' || buf[j-1] == '+'))
				reply += buf.substr(i, (j-1)-i);
			else
				reply += buf.substr(i, j-i);

			buf.clear();
			bufMtx.unlock();
			failureCount = 0;
			return reply;
		}
		// not done yet
		reply += buf.substr(i);
		i = buf.length();
		bufMtx.unlock();
	}
	if (failureCount > 2)
		logger.debug("Arduino: no reply to command (%d, %s)",
					  failureCount+1, s.c_str());
	if (failureCount++ > 20) {
		logger.fatal("lost contact with arduino");
		ready.store(false);
	}
	return "";
}

void Arduino::log() {
	string s = query("x");
	if (s.length() > 2)
		logger.trace("arduino log: %s", s.c_str());
}

int	Arduino::stressTest(int n, double period) {
	int count = 0; double tmin = 10.; double tmax = 0.; double ttot = 0.;
	int miss = 0;
	for (int i = 0; i < n; i++) {
		double t0 = Util::elapsedTime();
		string s = query("e" + to_string(i));
		double t = Util::elapsedTime();

		double delay = t - t0;
		if (s.length() == 0) {
			miss++; 
			//query("eOOPS");
			//logger.error("log: %s", query("x").c_str());
		} else {
			tmax = max(tmax, delay);
			ttot += delay;
			tmin = min(tmin, delay);
			int ii = atoi(s.c_str());
			if (ii != i) {
				count++;
				logger.error("bad return value (%d, %d)", i, ii);
			}
		}

		//if (i % 20 == 19) query("x");

		t = Util::elapsedTime();
		delay = t - t0;
		if (delay < period)
			sleep_for(milliseconds((int) (1000 * (period - delay))));
	}
	logger.error("tmin=%d tavg=%d tmax=%d", tmin, ttot/(n-miss), tmax);
	return count+miss;
}

} // ends namespace
