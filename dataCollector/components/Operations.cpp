/** @file Operations.cpp 
 *
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#include "Util.h"

#include "Pump.h"
#include "SupplyPump.h"
#include "Valve.h"
#include "MixValves.h"
#include "Status.h"
#include "Spectrometer.h"

#include "Config.h"
#include "Console.h"
#include "ConsoleInterp.h"
#include "DataStore.h"
#include "Operations.h"
#include "Interrupt.h"

using this_thread::sleep_for;

namespace fizz {

extern Logger logger;
extern Pump samplePump;
extern SupplyPump referencePump;
extern SupplyPump reagent1Pump;
extern SupplyPump reagent2Pump;
extern Valve portValve;
extern Valve filterValve;
extern MixValves mixValves;
extern Spectrometer spectrometer;
extern Status hwStatus;
extern Config config;
extern Interrupt interrupt;

/** Turn off all pumps, lights and leave valves in safe configuration
 */
void Operations::idleMode() {
	logger.details("going to idle mode");

	samplePump.off(); referencePump.off();
	filterValve.select(0); portValve.select(0);

	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.off(); reagent2Pump.off(); mixValves.select(0,0);
	}

	spectrometer.setLights(0b000);
}

/** Fill the spectrometer waveguide with fluid from reference reservoir.
 *  @param volume is the number of milliliters to be pumped.
 *  @param pumpRate is the reference pump rate in milliliters per minute.
 */
void Operations::referenceSample(double volume, double refPumpRate,
												   double samplePumpRate) {
	logger.details("referenceSample(%.2fml, %.2fml/m, %.2fml/m)",
					volume, refPumpRate, samplePumpRate);

	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(0, 0);
	}
	if (referencePump.isEnabled() && referencePump.available() >= volume) {
		referencePump.on(refPumpRate);
		interrupt.pause(60*(volume/refPumpRate));
		referencePump.off();
	} else {
		logger.error("running out of reference fluid, "
					 "switching to filtered seawater");
		referencePump.disable();
		filterValve.select(1);
		samplePump.on(samplePumpRate);
		interrupt.pause(60*(volume/samplePumpRate));
		samplePump.off();
		filterValve.select(0);
	}
}

/** Optimize the integration time to maximize sensitivity of spectrometer.
 *  Run the optimization procedure until successful, adding 1 ml of reference
 *  fluid after each unsuccessful attempt; give up after 5 attempts.
 *  @return true on success
 */
bool Operations::optimizeIntegrationTime(double volume, double refPumpRate,
										 double samplePumpRate) {
	bool validIntTime = spectrometer.adjustIntTime();
	for (int i = 0; i < 5 && !validIntTime; i++) {
		if (i < 3 && referencePump.isEnabled() &&
					 referencePump.available() >= volume) {
			referencePump.on(refPumpRate);
			interrupt.pause(60*(volume/refPumpRate));
			referencePump.off();
		} else {
			if (i == 3)
				logger.warning("optimizeIntegrationTime: no valid time using "
							   "ref fluid, disabling ref pump");
			referencePump.disable();
			filterValve.select(1);
			samplePump.on(samplePumpRate);
			interrupt.pause(60*(volume/samplePumpRate));
			samplePump.off();
			filterValve.select(0);
		}
		validIntTime = spectrometer.adjustIntTime();
	}
	if (validIntTime) {
		logger.details("optimizeIntegrationTime returning, "
					   "integrationTime=%.2f", spectrometer.getIntTime());
	} else {
		logger.details("referenceSample returning, no valid integrationTime");
	}
	return validIntTime;
}

/** Compute pump rates based on fractions associated with reagents1, 2.
 *  @param totalRate total flow rate from all three pumps.
 *  @param r1Frac fraction of total volume for reagent1.
 *  @param r2Frac fraction of total volume for reagent2.
 *  @param spRate is the desired samplePump rate on return.
 *  @param r1Rate is the desired reagent1Pump rate on return.
 *  @param r2Rate is the desired reagent2Pump rate on return.
 */
void Operations::computePumpRates(double& totalRate,
		double r1Frac, double r2Frac,
		double& spRate, double& r1Rate, double& r2Rate) {
	logger.trace("computePumpRates(%.2fml/m, %.3f, %.3f)",
			  totalRate, r1Frac, r2Frac);

	if (config.getHardwareConfig() == Config::BASIC) {
		spRate = min(totalRate, samplePump.getMaxRate());
		totalRate = spRate;
	} else {
		r1Rate = r1Frac * totalRate;
		r2Rate = r2Frac * totalRate;
		spRate = totalRate - (r1Rate + r2Rate);
	
		double scale = max(max(spRate/samplePump.getMaxRate(),
					r1Rate/reagent1Pump.getMaxRate()),
					r2Rate/reagent2Pump.getMaxRate());
	
		if (scale > 1.0) {
			spRate /= scale; r1Rate /= scale; r2Rate /= scale;
			totalRate /= scale;
		}
	}

	logger.trace("computePumpRates returns %.2fml/m, %.2fml/m, %.2fml/m",
		   	spRate, r1Rate, r2Rate);
}

/** Fill the spectrometer waveguide with an unfiltered seawater sample,
 *  possibly mixed with reagents.
 *  @param volume the total volume to be pumped (ml)
 *  @param totalRate rate of total flow through spectrometer.
 *  @param r1Frac fraction of pumped volume for reagent1.
 *  @param r2Frac fraction of pumped volume for reagent2.
 */
void Operations::unfilteredSample(double volume, double totalRate,
				  double r1Frac, double r2Frac) {
	logger.details( "unfilteredSample(%.2fml, %.2fml/m, %.3f, %.3f)", 
			volume, totalRate, r1Frac, r2Frac);

	if (reagent1Pump.available() < r1Frac * volume) {
		logger.warning("running out of reagent 1");
		throw EmptyReservoirException();
	}
	if (reagent2Pump.available() < r2Frac * volume) {
		logger.warning("running out of reagent 2");
		throw EmptyReservoirException();
	}

	double absTotalRate, spRate, r1Rate, r2Rate;
	absTotalRate = abs(totalRate);
	computePumpRates(absTotalRate, r1Frac, r2Frac, spRate, r1Rate, r2Rate);
	if (totalRate < 0) spRate = -spRate;

	filterValve.select(0);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(r1Rate > 0, r2Rate > 0);
	}
	samplePump.on(spRate);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.on(r1Rate); reagent2Pump.on(r2Rate);
		// note: on method with rate==0 is equivalent to off;
	}

	interrupt.pause(60*(volume/absTotalRate));

	// turn off pumps and restore valves to default configuration;
	samplePump.off();
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.off(); reagent2Pump.off(); mixValves.select(0, 0);
	}

	logger.trace("unfilteredSample returning");
}

/** Fill the spectrometer waveguide with a filtered seawater sample,
 *  possibly mixed with reagents. Throws PressureException if
 *  over-pressure condition is detected.
 *
 *  @param volume is the time to run the pumps (in seconds).
 *  @param totalRate is total rate of flow through spectrometer in ul/s.
 *  @param r1Frac fraction of total volume for reagent1.
 *  @param r2Frac fraction of total volume for reagent2.
 */
void Operations::filteredSample(double volume, double totalRate,
				double r1Frac, double r2Frac) {
	logger.details("filteredSample(%.2fml, %.2fml/m, %.3f, %.3f)",
			volume, totalRate, r1Frac, r2Frac);
	double absTotalRate, spRate, r1Rate, r2Rate;
	absTotalRate = abs(totalRate);
	computePumpRates(absTotalRate, r1Frac, r2Frac, spRate, r1Rate, r2Rate);
	if (totalRate < 0) spRate = -spRate;

	filterValve.select(1);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(r1Frac > 0, r2Frac > 0);
	}

	samplePump.on(spRate);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.on(r1Rate); reagent2Pump.on(r2Rate);
	}

	double startTime = Util::elapsedTime();
	double finishTime = startTime + (60.*(volume/absTotalRate));

	if (hwStatus.overPressure()) throw PressureException();
	double pressureCheckTime = startTime;

	interrupt.check();
	double now = Util::elapsedTime();
	while (now < finishTime) {
		if (now - pressureCheckTime > .5) { // check every 500 ms
			// overPressure() has delay of about 100 ms and
			// runs check() twice
			if (hwStatus.overPressure())
				throw PressureException();
			pressureCheckTime = now;
		} else {
			interrupt.pause(min(0.5, finishTime - now));
			//sleep_for(milliseconds(min(500, (int) (1000*(finishTime-now)))));
			//interrupt.check();
		}
		now = Util::elapsedTime();
	}

	// turn off pumps and restore valves to default configuration;
	samplePump.off();
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.off(); reagent2Pump.off();
	}

	filterValve.select(0);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(0, 0);
	}
	logger.trace("filteredSample returning");
}

/** Collect a filtered sample while adapting pump rate to avoid over-pressure.
 *  Throws PressureException if over-pressure condition is detected.
 *  @param totalVolume is the total volume to be pumped in ml,
 *  including reagents.
 *  @param r1Frac is fraction of volume for reagent1.
 *  @param r2Frac is fraction of volume for reagent2.

Notes.

This method was adapted from opd and has not been used in physs.
Code is pretty convoluted and am not confident it really makes sense.
Should be re-examined before using it in real deployment.

Summary

While pumping, check the pressure every 5 seconds, adjust the
rates to maintain pressure close to target; done using an
iterative adjustment with a limit of 5 iterations, with 50 ms
per iteration; also check for overpressure every 5 seconds

Might be simpler to adjust rates continuously every 50 ms,
but perhaps it's better to maintain stable rates.

 */
void Operations::filteredSampleAdaptive(double totalVolume,
					double r1Frac, double r2Frac) {

	logger.details("filteredSampleByVolume(%.1f ml, %.3f, %.3f)",
					totalVolume, r1Frac, r2Frac); 

	if (reagent1Pump.available() < r1Frac * totalVolume) {
		logger.warning("running out of reagent 1");
		throw EmptyReservoirException();
	}
	if (reagent2Pump.available() < r2Frac * totalVolume) {
		logger.warning("running out of reagent 2");
		throw EmptyReservoirException();
	}

	double spFrac = 1.0 - (r1Frac + r2Frac);

	double maxRate = samplePump.getMaxRate();
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		maxRate = min(min(maxRate,
				 reagent1Pump.getMaxRate()),
			   	 reagent2Pump.getMaxRate());
	}
	double minRate = maxRate/100;

	double targetPressure = config.getMaxPressure() / 2.;

	// select filter and set mixing valves based on pump rates;
	filterValve.select(1);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(r1Frac > 0, r2Frac > 0);
	}

	double rate = maxRate/5;  // start low to reduce chance of overpressure
	double pumpedVolume = 0.;
	while (pumpedVolume <  totalVolume) {
		logger.debug("Pumped %.2f out of %.2f", pumpedVolume, totalVolume);

		double spRate = rate * spFrac;
		double r1Rate = rate * r1Frac;
		double r2Rate = rate * r2Frac;
		samplePump.on(spRate);
		if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
			reagent1Pump.on(r1Rate); reagent2Pump.on(r2Rate);
		}

		double pressure = hwStatus.filterPressure();
		logger.debug("Pump rates: %.2f %.2f %.2f, pressure: %.1f",
					 spRate, r1Rate, r2Rate, pressure);
		if (hwStatus.overPressure())
			throw PressureException();

		double squirt; // amount pumped this iteration
		if (!adjustRates(rate, minRate, maxRate, r1Frac, r2Frac, 
				 targetPressure, squirt)) {
			throw PressureException();
		}
		if (rate >= 0) {
			interrupt.pause(5);
			// note: this method only checks for overPressure
			// every 5 seconds, but adjustRates checks every .5 sec;
			// since rate returned by adjustRates is chosen to
			// keep us close to targetPressure, we should be safe
			squirt += 5.0 * (spRate + r1Rate + r2Rate);
		}
		pumpedVolume += squirt;
	}

	// turn off pumps and restore valves to default configuration;
	samplePump.off();
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.off(); reagent2Pump.off();
	}
	filterValve.select(0);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(0, 0);
	}
	logger.trace("filteredSampleByVolume returning");
}

/** Attempt to adjust pump rates to achieve a specified target filter pressure.
 *  @param currentRate is the current total pump rate on input; it is
 *  modified by this method.
 *  @param minRate is the minimum allowed value for currentRate.
 *  @param maxRate is the maximum allowed value for currentRate.
 *  @param r1Frac is the fraction of the pumped volume for reagent1.
 *  @param r2Frac is the fraction of the pumped volume for reagent2.
 *  @param targetPressure is the filter pressure target to use in the
 *  control loop; should be substantially smaller than the maximum
 *  safe filter pressure.
 *  @param volPumped is the total volume pumped during execution of this
 *  method.
 *  @param return false
 */
bool Operations::adjustRates(double& currentRate,
		 double minRate, double maxRate, double r1Frac, double r2Frac,
		 double targetPressure, double& volPumped) {
	logger.trace("Operations::adjustRates(%.2fml/m, %.2fml/m, %.2fml/m, "
			 "%.3f, %.3f, %.1fpsi, %.1fml",
			 currentRate, minRate, maxRate, r1Frac, r2Frac,
			 targetPressure, volPumped);
	double spRate, r1Rate, r2Rate;
	computePumpRates(currentRate, r1Frac, r2Frac, spRate, r1Rate, r2Rate);

	volPumped = 0;
	for (int i = 0; i < 5; i++) {
		if (hwStatus.overPressure())
			throw PressureException();

		double currentPressure = hwStatus.filterPressure();
		double correction = currentRate *
			((targetPressure - currentPressure) / targetPressure);
		if ((correction > -.0001 && correction < .0001) ||
			(correction > 0 && currentRate >= maxRate))
			break;
		double newRate = currentRate + correction;
		newRate = min(newRate, maxRate);
		newRate = max(newRate, minRate);

		if (newRate != currentRate) {
			computePumpRates(newRate, r1Frac, r2Frac, spRate, r1Rate, r2Rate);
			samplePump.on(spRate);
			if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
				reagent1Pump.on(r1Rate); reagent2Pump.on(r2Rate);
			}
			currentRate = newRate;
		}
		interrupt.pause(.5);
		volPumped += spRate + r1Rate + r2Rate;
	}
	return true;
}

void Operations::flushFilter() {
	logger.debug("flushFilter() not yet implemented");
}

/** Flush system at end of sample cycle.
 *  First, flush mixing coils with seawater (1 ml each), then flush filter
 *  and spectrometer with seawater (2 ml), then flush with reference fluid
 *  (1 ml). The last step ensures that waveguide contains reference fluid during
 *  the idle time between sample cycles, to prevent stuff from growing in
 *  the waveguide. It's important that the port valves are switched before
 *  the flush takes place, so that when the next cycle starts, the intake
 *  tubing does not contain reference fluid.
 */
void Operations::flush() {
	filterValve.select(0);
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		logger.details("flushing mixing coils");
		samplePump.on(4);
		mixValves.select(1,0); interrupt.pause(10.0); 
		mixValves.select(1,1); interrupt.pause(5.0); 
		mixValves.select(0,1); interrupt.pause(10.0);
	}

	logger.details("flushing filter and waveguide");
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		mixValves.select(0, 0);
	}
	samplePump.on(4); interrupt.pause(30); samplePump.off();

	if (referencePump.isEnabled()) {
		referencePump.on(4); interrupt.pause(15); referencePump.off();
	}

/*
Note: we flush with unfiltered water because we do not want to
leave concentrated particulates behind in the filter. But this means
that we do leave particulates in the tubing leading up to where 
the reference fluid comes in. This creates the potential for
particulates to escape that section of tubing and enter the
waveguide during the next reference cycle. This can cause us
to increase the integration time excessively, and when the
particulates are washed out later, we get negative absorption
readings. Of course, this should happen rarely, since the
reference fluid should carry these particulates all the way
out through the waveguide. It will tend to happen if particulates
enter the waveguide late in the reference cycle, allowing them
to stay there.

Possible solutions
- during reference cycle, run sample pump in reverse at low
  speed to prevent particulates from from escaping into the
  forward flow
- during last phase of flush, run sample pump in reverse at
  low speed

Both solutions can bring reference fluid into the filter, which
may help keep the filter clean. The second may be more effective
at this, since it leaves the reference fluid in the filter until
the next sample cycle. The first just leaves it there until the
next sampling operation, following the reference cycle. So long
as no risk of damaging filter, seems like an ok thing to do,
and information we have says the filter can tolerate the dilute 
acid in the reference fluid.

Second solution probably needs more reference fluid if we really
want to fill the filter with reference fluid (depending on tubing
lengths). Should really have Jim produce a diagram with measurements
of the tubing lengths and volumes. Also, valve volumes.

Could start with the first solution, and possibly add the
second later if we have an issue with the filter pressure
building up. Seems like the more conservative choice.

Of course, the second choice is easier to implement.
*/

}

/** Purge bubbles
 *  Pump timings are based on tubing with an inside diameter of
 *  1/16" which gives a volume of .05 ml per inch of tubing.
 */
void Operations::purgeBubbles() {

	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		reagent1Pump.off(); reagent2Pump.off(); mixValves.select(0,0);
	}
	samplePump.off(); referencePump.off(); filterValve.select(0);
		// port valves are configured before calling

	logger.details("purging air bubbles");

	// run reference pump long enough to clear "stub"
	double rate = 4.0;
	if (referencePump.isEnabled()) {
		referencePump.on(rate); interrupt.pause(10.0); referencePump.off();
	}
	if (config.getHardwareConfig() == Config::TWO_REAGENTS) {
		// run reagent pumps long enough to clear bubbles from
		// supply tubing
		mixValves.select(1,0);
		reagent1Pump.on(rate); interrupt.pause(10.0); reagent1Pump.off();
		mixValves.select(0,1);
		reagent2Pump.on(rate); interrupt.pause(10.0); reagent2Pump.off();
	}
	flush(); // use flush to complete the purge, flush reagents and
		 // leave spectrometer with reference fluid
}

/** Acquire data that can be used to determine the best
 *  script parameters for producing a concentrated sample
 *  of particulates. First pump a filtered sample, then
 *  pump unfiltered sea water, stopping frequently to
 *  acquire a spectrum
 *  @param filtVol is the volume of fluid to pump for filtered
 *  sample
 *  @param filtRate is the rate at which to pump filtered sample
 *  @param unfVol is the volume to pump between  successive sample
 *  spectrum acquisitions
 *  @param unfTot is the total unfiltered volume to pump
 *  @param unfRate is the rate at which to pump unfiltered samples
 *  @return a string containing average transmission values for
 *  acquired spectra in range from 500-600 nm
 *
 *  This method is used by the console command of the same name,
 *  which can be invoked through the command line.
 */
string Operations::optimizeConcentration(double filtVol, double filtRate,
										 double unfVol, double unfTot,
										 double unfRate) {
	// pump a filtered sample
	filterValve.select(1);
	samplePump.on(filtRate);
	interrupt.pause((filtVol / filtRate) * 60);
	samplePump.off();

	// acquire baseline spectrum
	spectrometer.getSpectrum(0b111);
	double sum = 0; int cnt = 0;
	for (int i = 0; i < Spectrometer::SPECTRUM_SIZE; i++) {
		if (spectrometer.wavelengths[i] >= 500 &&
			spectrometer.wavelengths[i] < 600) {
			sum += spectrometer.spectrum[i]; cnt++;
		}
	}
	char buf[20];
	snprintf(buf, sizeof(buf), "%7.1f", sum/cnt);
	string s = string(buf);

	// pump sequence of unfiltered samples
	filterValve.select(0);
	for (double vol = 0; vol <= unfTot; vol += unfVol) {
		samplePump.on(unfRate);
		interrupt.pause((unfVol / unfRate) * 60);
		samplePump.off();
		spectrometer.getSpectrum(0b111);

		sum = 0;
		for (int i = 0; i < Spectrometer::SPECTRUM_SIZE; i++) {
			if (spectrometer.wavelengths[i] >= 500 &&
				spectrometer.wavelengths[i] < 600)
				sum += spectrometer.spectrum[i];
		}
		snprintf(buf, sizeof(buf), " %7.1f", sum/cnt);
		s += string(buf);
	}
	return s;
}

} // ends namespace
