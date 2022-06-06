SHELL := /bin/bash
IDIR := ../include
IDIR2 := ../include/seabreeze
CXXFLAGS := -Wall -O2 -std=c++0x

LIB := ../lib-fizz.a

HFILES = ${IDIR}/stdinc.h ${IDIR}/MixValves.h ${IDIR}/Pump.h \
	${IDIR}/Spectrometer.h ${IDIR}/SupplyPump.h \
	${IDIR}/Valve.h ${IDIR}/stdinc.h ${IDIR}/PowerControl.h \
	${IDIR}/Arduino.h ${IDIR}/LocationSensor.h \
	${IDIR}/Coord.h ${IDIR}/Status.h ${IDIR}/Clock.h
OFILES = MixValves.o Pump.o SupplyPump.o Valve.o Spectrometer.o \
	Arduino.o LocationSensor.o Status.o Clock.o

${OFILES} : ${HFILES}

Spectrometer.o: Spectrometer.cpp
	${CXX} ${CXXFLAGS} -Wno-deprecated -I ${IDIR} -I ${IDIR2} -c $<

.cpp.o:
	${CXX} ${CXXFLAGS} -I ${IDIR} -I ${IDIR2} -c $<

all:    
	make ${LIB}

${LIB} : ${OFILES}
	ar -ru ${LIB} ${OFILES}

clean:
	rm -f *.o
