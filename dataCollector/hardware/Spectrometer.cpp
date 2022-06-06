/** @file Spectrometer.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include <math.h>
#include "Spectrometer.h"
#include "Arduino.h"
#include "Config.h"
#include "CollectorState.h"
#include "Interrupt.h"

namespace fizz {

extern Logger logger;
extern Arduino arduino;
extern Config config;
extern CollectorState cstate;
extern Interrupt interrupt;

Spectrometer::Spectrometer() {
	intTime = 100;
	topRange.lo = 56000; topRange.mid = 58000; topRange.hi = 60000;
	spectrum.resize(SPECTRUM_SIZE, 0.);
	wavelengths.resize(SPECTRUM_SIZE, 0.);
}

Spectrometer::~Spectrometer() {
	if (noSpect) return;
	int errorCode;
	sb->closeDevice(spectId, &errorCode);
	SeaBreezeAPI::shutdown();
}

/** Initialize spectrometer hardware. */
bool Spectrometer::initDevice() {
	unique_lock<mutex> lck(spectMtx);
	return privateInitDevice();
}

/** Initialize spectrometer hardware.
 *  Private version for use by method that already hold lock.
 */
bool Spectrometer::privateInitDevice() {
	privateSetLights(0b000);

	// initialize simulated wavelengths array
	// this gets over-written when spectrometer is present
	for (unsigned int i = 0; i < SPECTRUM_SIZE; i++) {
		double frac = ((double) i) / SPECTRUM_SIZE;
		wavelengths[i] = 100.0 + 800.0 * frac;
	}
	i440 = 400; i580 = 600;

	sb = SeaBreezeAPI::getInstance();
	noSpect = false;
	// get device id and spectrometer id from spectrometer
	if (sb->probeDevices() < 1) {
		cerr << "Spectrometer:: no spectrometer device detected\n";
		noSpect = true; return false;
	}
	if (sb->getNumberOfDeviceIDs() < 1) {
		cerr << "Spectrometer:: no device IDs detected\n";
		noSpect = true; return false;
	}
	long idvec[10];
	if (sb->getDeviceIDs(idvec, 1) < 1) {
		cerr << "Spectrometer:: no device IDs returned\n";
		noSpect = true; return false;
	}
	deviceId = idvec[0];

	int errorCode;
	if (sb->getDeviceType(deviceId, &errorCode, deviceType, 20) != 0) {
		cout << "Spectrometer:: device type: " << deviceType << endl;
	} else {
		cerr << "Spectrometer:: could not read device type " << endl;
		noSpect = true; return false;
	}

	if (sb->openDevice(deviceId, &errorCode) != 0) {
		cerr << "Spectrometer:: could not open device" << endl;
		noSpect = true; return false;
	}

	long features[10];
	if (sb->getNumberOfSerialNumberFeatures(deviceId, &errorCode) < 1 ||
		sb->getSerialNumberFeatures(deviceId, &errorCode,
				features, 10) < 1 ||
		sb->getSerialNumber(deviceId, features[0],
				&errorCode, serialNumber, 20) < 1) {
		cerr << "Spectrometer:: could not read spectrometer serial "
			"number" << endl;
		noSpect = true; return false;
	}

	cout << "Spectrometer:: serial number is " << serialNumber << endl;

	if (sb->getNumberOfSpectrometerFeatures(deviceId, &errorCode) < 1 ||
		sb->getSpectrometerFeatures(deviceId, &errorCode,
				features, 10) < 1) {
		cerr << "Spectrometer:: cannot obtain spectrometer's "
				"identifier" << endl;
		noSpect = true; return false;
	}
	spectId = features[0];

	double wave[SPECTRUM_SIZE];
	if (sb->spectrometerGetWavelengths(deviceId, spectId, &errorCode,
			wave, SPECTRUM_SIZE) != SPECTRUM_SIZE) {
		cerr << "Spectrometer:: could not read wavelengths" << endl;
		noSpect = true; return false;
	}
	for (unsigned int i = 0; i < SPECTRUM_SIZE; i++) wavelengths[i] = wave[i];

	// let i440 be the largest index with wavelengths[i440] <= 440
	i440 = (int) ((440.0 - wavelengths[0])
		   / (wavelengths[SPECTRUM_SIZE-1] - wavelengths[0]))
		   * SPECTRUM_SIZE;
	while (wavelengths[i440] > 440) i440 -= 1;
	while (wavelengths[i440+1] <= 440) i440 += 1;

	// let i580 be the largest index with wavelengths[i580] <= 580
	i580 = (int) ((580.0 - wavelengths[0])
		   / (wavelengths[SPECTRUM_SIZE-1] - wavelengths[0]))
		   * SPECTRUM_SIZE;
	while (wavelengths[i580] > 580) i580 -= 1;
	while (wavelengths[i580+1] <= 580) i580 += 1;

	double coefs[15]; int numCoef = 0;
	if (sb->getNumberOfNonlinearityCoeffsFeatures(deviceId,&errorCode)>0 &&
		sb->getNonlinearityCoeffsFeatures(deviceId,&errorCode,features,10)>0) {
		numCoef = sb->nonlinearityCoeffsGet(deviceId, features[0],
										    &errorCode, coefs, 15);
	}
	if (numCoef <= 0) {
		cerr << "Spectrometer:: unable to read nonlinearity correction "
			 << "coefficients\n";
	} else {
		for (int i = 0; i < numCoef; i++) corrCoef.push_back(coefs[i]);
	}

	srand((int) 1000000 * Util::elapsedTime());
		// random numbers used to generate simulated spectra
		// when no spectrometer available

	return true;
}

/** Initialize state variables.  */
void Spectrometer::initState() {
	unique_lock<mutex> lck(spectMtx);
	intTime = cstate.getIntegrationTime();
	privateSetIntTime(intTime);
} 

/** Acquire a spectrum.
 *  Actually returns the average of 10 individual spectra.
 *  @param lconfig is the light configuration (deterium, tungsten, shutter)
 *  @param spect points to an array of SPECTRUM_SIZE doubles, in which
 *  the spectrum is returned.
 *  @return true on success, false on failure.
 */
bool Spectrometer::getSpectrum(int lconfig) {
	unique_lock<mutex> lck(spectMtx);
	logger.details("getSpectrum(%d)", lconfig);
	bool status = privateGetSpectrum(lconfig);
	logger.trace("getSpectrum returning");
	return status;
}

/** Acquire a spectrum without acquiring a lock.
 *  Private method, used by other methods that already hold the lock.
 *  @param lconfig is the light configuration (deterium, tungsten, shutter)
 *  @return true on success, false on failure; on a successful return,
 *  the acquired spectrum is in this->spectrum and the values of
 *  spectAvg, spectMax and waveMax have been computed.
 */
bool Spectrometer::privateGetSpectrum(int lconfig) {
	if (lconfig&1) privateSetLights(1); // open shutter early
	interrupt.pause(2);	// to prevent rapid cycling
	privateSetLights(lconfig);
	interrupt.pause(2);	// to let light source stabilize
	if (noSpect) {
		// return dummy spectrum
		if ((lconfig & 1) == 0 || (lconfig & 6) == 0) {
			for (unsigned int i = 0; i < spectrum.size(); i++)
				spectrum[i] = 2000.0 + (rand() % 200);
		} else {
			for (unsigned int i = 0; i < spectrum.size(); i++) {
				spectrum[i] = (45000. - .4 * pow(wavelengths[i] - 500., 2.))
							   + 10000 * sin(12 * 3.14 * i / spectrum.size()) +
							   + (rand() % 2000);
				spectrum[i] = max(0.0, spectrum[i]);
				spectrum[i] = min(60000.0, spectrum[i]);
			}
		}
	} else {
		int errorCode;
		double spect[spectrum.size()];
		spectrum.assign(spectrum.size(), 0.0);
		for (int i = 0; i < 10; i++) {
			interrupt.check();
			int ssize = sb->spectrometerGetFormattedSpectrum(
				deviceId, spectId, &errorCode, spect, SPECTRUM_SIZE);
			if ((unsigned) ssize != spectrum.size()) {
				logger.error("Spectrometer::getSpectrum: "
						 	 "unexpected spectrum length: %d", ssize);
				return false;
			}
			spect[0] = spect[1] = 0; // ignore spurious values
			for (unsigned int j = 0; j < spectrum.size(); j++)
				spectrum[j] += spect[j];
		}
		for (unsigned int j = 0; j < spectrum.size(); j++)
			spectrum[j] /= 10;
	}
	spectAvg = 0; spectMax = 0.; waveMax = 0.;
	for (unsigned int j = 0; j < spectrum.size(); j++) {
		if (spectrum[j] > spectMax) {
			spectMax = spectrum[j]; waveMax = wavelengths[j];
		}
		spectAvg += spectrum[j];
	}
	spectAvg /= spectrum.size();
	logger.details("spectrum: avg=%.0f, max=%.0f, maxWave=%.0f, "
				   "i440=%.0f intTime=%.1f",
			   	   spectAvg, spectMax, waveMax, spectrum[i440], intTime);
	privateSetLights(0b000);
	spectrum[0] = 0;
	return true;
}

/** Set integration time in milliseconds.
 *  @param itime is the specified integration time
 */
void Spectrometer::setIntTime(double itime) {
	privateSetIntTime(itime);
	cstate.setIntegrationTime(intTime);
}

/** Set integration time in milliseconds.
 *  Private version for methods that already hold lock.
 *  @param itime is the specified integration time
 */
void Spectrometer::privateSetIntTime(double itime) {
	if (!noSpect) {
		unsigned long ltime = (unsigned long) (1000 * itime);
		int errorCode;
		sb->spectrometerSetIntegrationTimeMicros(deviceId, spectId,
						 						 &errorCode, ltime);
	}
	intTime = itime;
}

/** Control light sources.
 *
 *  @param lconfig specifies the configuration of the lights;
 *  if bit 2 is 1, deuterium is on, else off;
 *  if bit 1 is 1, tungsten is on, else off;
 *  if bit 0 is 1, tungsten is on, else off;
 */
void Spectrometer::setLights(int lconfig) {
	unique_lock<mutex> lck(spectMtx);
	logger.trace("Spectrometer::setLights(config=%s)",
			 	 Util::bits2string(lconfig,3).c_str());
	privateSetLights(lconfig);
}

void Spectrometer::privateSetLights(int lconfig) {
	arduino.send("l" + Util::bits2string(lconfig, 3));
	(this->lconfig) = lconfig;
}

int Spectrometer::getLights() {
	return lconfig;
		// no locking, since only used for gui display and don't
		// want to wait during spectrum acquisition
}

/** Adjusts the integration time for the spectrometer.
 *
 *  The integration time is the length of time to keep the light source
 *  on when measuring a sample. If integration time is too long, the light
 *  sensors become saturated and we don"t get useful data. Still, we want
 *  the readings for cdomRef to be near the high end of the scale in order
 *  to maximize the sensitivity.
 *
 *  @param intTime is initially, the current integration time (in ms); it's
 *  value is updated to maximize spectrometer sensitivity;
 *  @return true if final value of intTime produces light levels within target 
 *  range
 */
bool Spectrometer::adjustIntTime() {
    logger.trace( "adjustIntTime()", intTime);

	for (int i = 0; i < 10; i++) {
    	getSpectrum(0b111);
		if (spectMax > topRange.hi) {
			setIntTime(max(5., intTime / 2.));
		} else if (spectMax < topRange.lo) {
			if (intTime >= 500) return false;
			setIntTime(min(500.0, intTime * topRange.mid/spectMax));
		} else {
			break;
		}
	}
	logger.trace("adjustIntTime returning (true, %.2f)", intTime);
	return true;
}

bool Spectrometer::checkLights() {
	unique_lock<mutex> lck(spectMtx);
	logger.details("Spectrometer::checkLights()");
	int lconfig = this->lconfig;

	if (noSpect) return true;

	if (!privateGetSpectrum(0b110)) {
		logger.error("Spectrometer::checkLights: unable to acquire spectrum");
		return false;
	}
	double dark440 = spectrum[i440];
	double dark580 = spectrum[i580];

	bool status = true;

	if (!privateGetSpectrum(0b101) || spectrum[i440] < dark440 + 200) {
		logger.error("Spectrometer::checkLights: deuterium light "
				 "source failure");
		status = false;
	}

	if (!privateGetSpectrum(0b011) || spectrum[i580] < dark580 + 200) {
		logger.error("Spectrometer::checkLights: tungsten light "
				 	 "source failure");
		status = false;
	}

	privateSetLights(lconfig);

	return status;
}

} // ends namespace
