/** @file Util.h
 *
 *  @author Jon Turner
 *  @date 2016
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#ifndef UTIL_H
#define UTIL_H

#include "stdinc.h"
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

namespace fizz {

/** This class contains miscellaneous utility methods.
 */
class Util {
public:
	static bool prefix(string&, string&);	
	static int strnlen(char*, int);
	static void split(string&, int, vector<string>&);
	static double elapsedTime();
	static string bits2string(int, int);
	static int string2bits(const string&);
};

} // ends namespace

#endif
