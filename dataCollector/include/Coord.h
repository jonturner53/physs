/** \file Coord.h
 *  @author Jon Turner
 *  @date 2019
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef COORD_H
#define COORD_H

#include "stdinc.h"
#include <mutex>
#include <atomic>

namespace fizz {

class Coord {
public:
	double	lat;
	double	lon;

	string	toString() {
		stringstream ss;
		ss << std::fixed << std::setprecision(4)
		   << "["  << (lat >= 0 ? "N" : "S") << abs(lat)
		   << ", " << (lon >= 0 ? "E" : "W") << abs(lon) << "]";
		return ss.str();
	};

	void fromString(string& s) {
		lat = lon = 0.;
		const char *p = s.c_str();
		int i = s.find_first_of("NS");
		if (i < 0) return;
		lat = strtod(p+i+1, NULL);
		if (p[i] == 'S') lat = -lat;
		i = s.find_first_of("EW");
		if (i < 0) return;
		lon = strtod(p+i+1, NULL);
		if (p[i] == 'W') lon = -lon;
	};
};

} // ends namespace

#endif
