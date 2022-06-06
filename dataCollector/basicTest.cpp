/** \file basicTest.cpp
 *  @author Jon Turner
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "stdinc.h"
#include <vector>
#include "Arduino.h"
#include "Util.h"
#include "Logger.h"

using std::this_thread::sleep_for;
using std::chrono::milliseconds;

using namespace fizz;

namespace fizz {

class Log2stderr : public LogTarget {
public:     Log2stderr(int level) : LogTarget(level) {}
    void    logMessage(const string& s, int level) {
        if (level >= logLevel) cerr << s;
    }
} log2stderr(Logger::DEBUG);
Logger logger;

}

// instantiate pins

int split(string&, string, int);
bool processCommand(string);
void pumpSpeed(int, int);

void startSpect();
void stopSpect();
bool getSpect(double*);
bool getWavelengths(double*);

/** Perform basic tests on the arduino version of the physs.
 * 
 * usage: aBasicTest
 * 
 * This program does some initialization, then reads commands from stdin
 * and responds as requested. Commands take one of the following forms.
 * 
 * help		display help message
 * power bb   	where each bit in bb specifies one of the power
 *		types (power to pumps/valves, light source);
 *		b=0 for off, 1 for on
 * lights bbb 	where each bit in bbb specifies one of the two
 *		light sources (deuterium, tungsten) and the shutter
 *		b=0 for off, 1 for on
 * valve i b  	where i is a valve (1-5) and b=0 for default branch, 1 for
 *		active branch (for three-way valves, b=0 for first position,
 *		1 for second position)
 * pump i s	where i is a pump # (1-4) and s is a pump speed (0 to 4095)
 * status   returns values of vbat, temp, pres1, pres2 and leak
 * detect	returns true if arduino is present
 * commLink 0|1	turn comm link on and off (by switching power to cell modem)
 * check4faults 0|1 turn automated fault detection on (1) or off (0)
 * sleep m..m turn processor off for m..m minutes, after a 30 second delay\
 * spectrum	read a spectrum and show selected values
 * wavelengths	read spectrometer wavelengths and show selected values
 * log		print arduino log messages
 * echo s	echos the string s (enclosed in double quotes) back to the
 * 		console
 * pause t	pauses for t seconds (t may include a decimal point)
 * quit		exits the program
 */

string helpString = "Commands\n\n"
"help        display help message\n"
"power bb    where each bit in bb specifies one of the power\n"
"            types (valve and pump power, light power);\n"
"            b=0 for off, 1 for on\n"
"lights bbb  where each bit in bbb specifies one of the two\n"
"            light sources (deuterium, tungsten) and the shutter\n"
"            b=0 for off, 1 for on\n"
"valve i b   where i is a valve (1-6) and b=0 for default branch, 1 for\n"
"            active branch (for three-way valves, b=0 for first position,\n"
"            1 for second position)\n"
"pump i s    where i is a pump # (1-6) and s is a pump speed (0 to 4095)\n"
"status      returns values of vbat, temp, pressure1, pressure2 and leak\n"
"time [ ss mm hh dd DD mm yy ] get or set time\n"
"detect      returns true if arduino is present\n"
"commLink 0|1 turn comm link on and off (by switching power to cell modem)\n"
"check4faults 0|1 turn fault checking on and off\n"
"sleep m..m  turn processor off for m..m minutes, after a 30 second delay\n"
"spectrum    read a spectrum and show selected values\n"
"wavelengths read spectrometer wavelengths and show selected values\n"
"log       	 print arduino log messages\n"
"echo s      echos the string s (enclosed in double quotes) back to the\n"
"            console\n"
"pause t     pauses for t seconds (t may include a decimal point)\n"
"quit        exits the program\n";

Arduino arduino;

int main() {
	logger.addTarget(log2stderr);
	arduino.start();
	startSpect();
	// add code to set pump speeds to 0
	while (true) {
		cout << "::";
		string line; 
		getline(cin, line);
		if (!processCommand(line)) break;
	}
	arduino.finish();
}

/** Split a string into words.
 *  @param line is a string to be split into words, where a word is any
 *  non-zero sequence of non-space characters
 *  @param words is an array in which the individual words are returned
 *  @param n is the length of the words array; if more than n words are
 *  present in line, only the first n are returned in the words array
 *  @return the number of words returned in the words array
 */
int split(string& line, string words[], int n) {
	unsigned int i = 0; int cnt = 0;
	while (cnt < n) {
		while (i < line.length() && isspace(line[i])) i++;
		int start = i;
		while (i < line.length() && !isspace(line[i])) i++;
		int len = i - start;
		if (len == 0) break;
		words[cnt++] = line.substr(start, len);
	}
	return cnt;
}

// macro used in processCommand
#define checkit(x) { \
	if (!(x)) { \
		cout << "command error: " << line << endl; \
		return true; \
	} \
}

/** Read a command from stdin and carry it out.
 *  @param line contains one line of input (no '\n')
 *  @return false if a quit command was issued, else true
 */
bool processCommand(string line) {
	string words[10];
	int n = split(line, words, 10);
	if (n == 0) return true;

	string& cmd = words[0];
	if (cmd != "echo" && cmd != "time") checkit(n <= 3);

	if (cmd == "help") {
		cerr << helpString;
	} else if (cmd == "power") {
		checkit(n == 2 && words[1].length() == 2);
		arduino.send(("P" + words[1]).c_str());
	} else if (cmd == "lights") {
		checkit(n == 2 && words[1].length() == 3);
		arduino.send(("l" + words[1]).c_str());
	} else if (cmd == "valve") {
		checkit(n == 3);
		int num = -1; int state = -1;
		try { num=stoi(words[1]); state=stoi(words[2]); } catch (...) {
			cout << "command error: " << line << endl;
			return true;
		}
		checkit(num >= 1 && num <= 6 && state >= 0 && state <= 1);
		arduino.send("v" + words[1] + words[2]);
	} else if (cmd == "pump") {
		checkit(n == 3);
		int num = -1; int speed = -1;
		try { num=stoi(words[1]); speed=stoi(words[2]); } catch (...) {
			cout << "command error: " << line << endl;
			return true;
		}
		checkit(num >= 1 && num <= 6 && speed >= 0 && speed <= 4095);
		arduino.send("p" + words[1] + words[2]);
	} else if (cmd == "status") {
		string s = arduino.query("s");
		cout << "status = " << s << endl;
	} else if (cmd == "time") {
		if (n == 8) {
			arduino.send("T " + words[1]  + " " + words[2]  + " "
							  + words[3]  + " " + words[4]  + " "
						      + words[5]  + " " + words[6]  + " "
							  + words[7]);
		} else {
			cout << arduino.query("t") << endl;
		}
	} else if (cmd == "detect") {
		string s = arduino.query("ehello");
		bool status = (s == "hello");
		if (status) {
			s = arduino.query("H");
			cout << "arduino is communicating (" + s + ")\n";
		} else {
			cout << "no reply from arduino\n";
		}
	} else if (cmd == "commLink") {
		checkit(n == 2 && words[1].length() == 1);
		if (words[1] == "0") {
			cout << "turning commLink off\n";
			arduino.send("M0");
		} else {
			cout << "turning commLink on\n";
			arduino.send("M1");
		}
	} else if (cmd == "check4faults") {
		checkit(n == 2 && words[1].length() == 1);
		if (words[1] == "0") {
			cout << "turning fault checking off\n";
			arduino.send("F0");
		} else {
			cout << "turning fault checking on\n";
			arduino.send("F1");
		}
	} else if (cmd == "sleep") {
		checkit(n == 2);
		cout << "putting Beaglebone to sleep for " << words[1] << " minutes\n";
		arduino.send("S" + words[1]);
	} else if (cmd == "spectrum") {
		double spect[2048];
		if (!getSpect(spect)) {
			cout << "unable to read spectrum" << endl;
		} else {
			cout << "sample of spectrum (1 in 200)\n";
			for (int i = 0; i < 2048; i += 200)
				cout << spect[i] << " ";
			cout << endl;
		}
	} else if (cmd == "wavelengths") {
		double wavelengths[2048];
		if (!getWavelengths(wavelengths)) {
			cout << "unable to read wavelengths" << endl;
		} else {
			cout << "sample of wavelengths (1 in 200)\n";
			for (int i = 0; i < 2048; i += 200)
				cout << wavelengths[i] << " ";
			cout << endl;
		}
	} else if (cmd == "log") {
		string s = arduino.query("x");
		cout << "arduino log messages:\n" << s << endl;
	} else if (cmd == "echo") {
		unsigned int i = line.find('"'); checkit(i!=string::npos);
		unsigned int j = line.find('"', i+1); checkit(j!=string::npos);
		string s = arduino.query("e" + line.substr(i+1, j-(i+1)));
		cout << s << endl;
	} else if (cmd == "stressTest") {
		checkit(n == 3);
		int n = atoi(words[1].c_str());
		double period = atof(words[2].c_str()) / 1000;
		int miss = arduino.stressTest(n, period);
		cout << "missed " << miss << " out of " << n << endl;
	} else if (cmd == "dpin") {
		checkit(n == 3 && words[1].length() <= 2 && words[2].length() == 1);
		if (words[1].length() == 1) words[1] = "0" + words[1];
		arduino.send("D" + words[1] + words[2]);
	} else if (cmd == "apin") {
		checkit(n == 2 && words[1].length() == 1);
		string s = arduino.query("A" + words[1]);
		cout << s << endl;
	} else if (cmd == "quit") {
		arduino.send("P000");
		return false;
	} else {
		cout << "command error: " << line << endl;
	}
	return true;
}

#include "api/seabreezeapi/SeaBreezeAPI.h"

SeaBreezeAPI* sb;
long deviceId;
long spectId;
bool noSpect;

void stopSpect() {
	int errorCode;
	sb->closeDevice(deviceId, &errorCode);
	SeaBreezeAPI::shutdown();
}

void startSpect() {
	sb = SeaBreezeAPI::getInstance();
	noSpect = false;

	if (sb->probeDevices() < 1) {
		cerr << "no spectrometer device detected\n";
		noSpect = true; return;
	}
	if (sb->getNumberOfDeviceIDs() < 1) {
		cerr << "no device IDs detected\n";
		noSpect = true; return;
	}
	long idvec[10];
	if (sb->getDeviceIDs(idvec, 1) < 1) {
		cerr << "no device IDs returned\n";
		noSpect = true; return;
	}
	deviceId = idvec[0];

	int errorCode;
	char buf[100];
	if (sb->getDeviceType(deviceId, &errorCode, buf, 100) != 0) {
		cout << "spectrometer device type: " << buf << endl;
	} else {
		cerr << "could not read device type " << endl;
		noSpect = true; return;
	}

	if (sb->openDevice(deviceId, &errorCode) != 0) {
		cerr << "could not open device" << endl;
		noSpect = true; return;
	}

	long features[10];
	if (sb->getNumberOfSerialNumberFeatures(deviceId, &errorCode) < 1 ||
	    sb->getSerialNumberFeatures(deviceId, &errorCode,
				features, 10) < 1 ||
	    sb->getSerialNumber(deviceId, features[0],
				&errorCode, buf, 100) < 1) {
		cerr << "could not read spectrometer serial number" << endl;
		noSpect = true; return;
	}

	cout << "spectrometer serial number is " << buf << endl;

	if (sb->getNumberOfSpectrometerFeatures(deviceId, &errorCode) < 1 ||
	    sb->getSpectrometerFeatures(deviceId, &errorCode,
				features, 10) < 1) {
		cerr << "no spectrometer defined for device" << endl;
		noSpect = true; return;
	}
	spectId = features[0];

	unsigned long minIntTime =
			sb->spectrometerGetMinimumIntegrationTimeMicros(
				deviceId, spectId, &errorCode);
	unsigned long maxIntTime =
			sb->spectrometerGetMaximumIntegrationTimeMicros(
				deviceId, spectId, &errorCode);
	cerr << "integration time range: " << minIntTime/1000. << "-" <<
			maxIntTime/1000 << " milliseconds\n";

	double coefs[15]; int numCoef = 0;
	if (sb->getNumberOfNonlinearityCoeffsFeatures(deviceId,&errorCode)>0 &&
		sb->getNonlinearityCoeffsFeatures(deviceId,&errorCode,features,10)>0) {
		numCoef = sb->nonlinearityCoeffsGet(deviceId, features[0],
										    &errorCode, coefs, 15);
	}
	if (numCoef <= 0) {
		cerr << "unable to read nonlinearity correction coefficients\n";
	} else {
		cerr << "nonlinearity correction coefficients\n";
		for (int i = 0; i < numCoef; i++) {
			if (i == 4) cerr << "\n";
			cerr << coefs[i] << " ";
		}
		cerr << "\n";
	}

	return;
}

bool getSpect(double* spect) {
	int errorCode;

	if (noSpect) return false;
	int length = sb->spectrometerGetFormattedSpectrumLength(
					deviceId, spectId, &errorCode);
	if (length != 2048) {
		cerr << "unexpected spectrum length\n";
		return false;
	}
	if (sb->spectrometerGetFormattedSpectrum(
		deviceId, spectId, &errorCode, spect, length) != 2048) {
		cerr << "could not read full spectrum" << endl;
		return false;
	}
	return true;
}

bool getWavelengths(double* wavelengths) {
	int errorCode;

	if (noSpect) return false;
	if (sb->spectrometerGetWavelengths(
		deviceId, spectId, &errorCode, wavelengths, 2048) != 2048) {
		cerr << "could not read wavelengths" << endl;
		return false;
	}
	return true;
}
