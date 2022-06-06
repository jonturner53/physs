/** \file Exceptions.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "stdinc.h" 
#include <exception>

namespace fizz {

/** Typically raised in response to a stop command. */
class InterruptException : public std::exception {
	public:
	const char* what() const throw() {
		return "InterruptException";
	}
};

/** Raised in response to a excessive pressure across filter. */
class PressureException : public std::exception {
	public:
	const char* what() const throw() {
		return "PressureException";
	}
};

/** Raised in response to a excessive pressure across filter. */
class EmptyReservoirException : public std::exception {
	public:
	const char* what() const throw() {
		return "EmptyReservoirException";
	}
};

} // ends namespace

#endif
