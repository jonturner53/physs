/** \file Spectrometer.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef SPECTROMETER_H
#define SPECTROMETER_H

#include<mutex>
#include<atomic>

#include "Logger.h" 
#include "api/seabreezeapi/SeaBreezeAPI.h"

namespace fizz {

/** This class provides an api for controlling a spectrometer.
 */
class Spectrometer {
public:
		Spectrometer();
		~Spectrometer();

	bool	initDevice();
	void	initState();

	string	getSerialNumber() { return string(serialNumber); };
    vector<double> getCorrectionCoef() { return vector<double>(corrCoef); };

	bool	getStatus() { return status; };
	bool	getSpectrum(int);
	double	getIntTime();
	void	setIntTime(double);
	void	setLights(int);
	int		getLights();
	bool	adjustIntTime();
	bool	checkLights();

	static const int SPECTRUM_SIZE = 2048; ///< number of values in spectrum
	vector<double> spectrum;		///< last spectrum read
	vector<double> wavelengths; 	///< vector of wavelengths
	vector<double> corrCoef;		///< nonlinearity correction coefficients

	double	spectAvg;		///< average value in spectrum
	double	spectMax;		///< largest value in spectrum
	double	waveMax;		///< wavelength with largest value

	char	deviceType[20];  ///< type of spectrometer
	char	serialNumber[20]; ///< spectrometer serial number
private:
	bool	status;			///< true if spectrometer is powered on
	double	intTime;
	int		lconfig;		///< bit 2 for deuterium, bit 1 for tungsten
							///< bit 0 for shutter

	SeaBreezeAPI* sb;	///< instance of seabreeze api
	long	deviceId;	///< identifier for hardware device
	long	spectId;	///< identifier for spectrometer

	struct { int lo, mid, hi; } topRange;

	int	i440;		///< index of largest wavelength <=440 nm
	int	i580;		///< index of largest wavelength <=580 nm

	bool	noSpect;	///< true if cannot detect/init spectrometer

	mutex	spectMtx;

	bool	privateInitDevice();
	bool	privateGetSpectrum(int);
	void	privateSetLights(int);
	void	privateSetIntTime(double);
};

inline double Spectrometer::getIntTime() {
	unique_lock<mutex> lck(spectMtx);
	return intTime;
}

} // ends namespace

#endif
