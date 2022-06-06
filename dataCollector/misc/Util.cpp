/** @file Util.cpp 
 *
 *  @author Jon Turner
 *  @date 2011
 *  This is open source software licensed under the Apache 2.0 license.
 *  See http://www.apache.org/licenses/LICENSE-2.0 for details.
 */

#include "Util.h"

namespace fizz {

/** Test if one string is a prefix of another.
 *  @param s1 is a reference to a string
 *  @param s2 is a reference to another string
 *  @return true if s1 is a non-empty prefix of s2, else false.
 */
bool Util::prefix(string& s1, string& s2) {
	return s1.length() > 0 && s2.find(s1) == 0;
}

/** Replacement for the missing strnlen function.
 */
int Util::strnlen(char* s, int n) {
	for (int i = 0; i < n; i++) 
		if (*s++ == '\0') return i;
	return n;
}

/** Divide a string into parts separated by whitespace.
 *  @param s is string to be split
 *  @param n is the maximum number of strings to be returned in parts;
 *  the last string may include whitespace.
 *  @param parts is a vector of strings in which result is returned.
 */
void Util::split(string& s, int n, vector<string>& parts) {
	string whitespace = " \t\f\v\r\n";
	
	parts.clear();
	if (n <= 0) return;
	size_t p = s.find_last_not_of(whitespace);
	if (p != string::npos) s.erase(p+1);
	p = s.find_first_not_of(whitespace);
	while (parts.size() < (unsigned) n-1 && p != string::npos) {
		size_t q = s.find_first_of(whitespace, p);
		if (q == string::npos) {
			parts.push_back(s.substr(p));
			return;
		}
		parts.push_back(s.substr(p, q-p));
		p = s.find_first_not_of(whitespace, q);
	}
	if (p != string::npos) parts.push_back(s.substr(p));
	return;
}

/** Return time expressed as a free-running microsecond clock
 *
 *  Uses the steady_clock in the <chrono> library.
 *  @return the number of seconds since the first call to elapsedTime().
 */
double Util::elapsedTime() {
	static bool first = true;
	static time_point<steady_clock> t0;

	time_point<steady_clock> now = steady_clock::now();

	if (first) { t0 = now; first = false; }

	duration<double> diff = duration_cast<duration<double>>(now - t0);

	return diff.count();
}

/** Create a binary string representation of an integer.
 *  @param x is an integer to be converted to a string.
 *  @param n is the number of bits to include in the string representation.
 *  @return a string representing bits n-1 down to 0.
 */
string Util::bits2string(int x, int n) {
	string s;
	for (int i = n-1; i >= 0; i--)
		s += ((x >> i) & 1) ? '1' : '0';
	return s;
}

/** Create an integer from a binary representation of a string.
 *  @param s is a string consisting of the characters '0' and '1' only;
 *  the length of s is assumed to be no more than the number of bits in
 *  an integer variable.
 *  @return an integer representation of s.
 */
int Util::string2bits(const string& s) {
	int x = 0;
	for (unsigned int i = 0; i < s.length(); i++) {
		x <<= 1; x |= (s[i] == '1' ? 1 : 0);
	}
	return x;
}

} // ends namespace
