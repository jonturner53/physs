SHELL := /bin/bash
IDIR := ../include
SBIDIR := ../include/seabreeze
BIN := /usr/local/bin
LIB := ../lib-fizz.a

CXXFLAGS := -Wall -O2 -std=c++0x

HFILES = ${IDIR}/stdinc.h ${IDIR}/Console.h ${IDIR}/ConsoleInterp.h \
	${IDIR}/ScriptInterp.h ${IDIR}/Operations.h ${IDIR}/Config.h \
	${IDIR}/CollectorState.h ${IDIR}/DataStore.h ${IDIR}/Interrupt.h \
	${IDIR}/MaintLog.h
OFILES = Config.o Console.o Interrupt.o CollectorState.o DataStore.o \
	Operations.o ScriptInterp.o ConsoleInterp.o MaintLog.o

${OFILES} : ${HFILES}

ConsoleInterp.o : ConsoleInterp.cpp
	${CXX} ${CXXFLAGS} -Wno-deprecated -I ${IDIR} -I ${SBIDIR} -c $<
DataStore.o : DataStore.cpp
	${CXX} ${CXXFLAGS} -Wno-deprecated -I ${IDIR} -I ${SBIDIR} -c $<
Operations.o : Operations.cpp
	${CXX} ${CXXFLAGS} -Wno-deprecated -I ${IDIR} -I ${SBIDIR} -c $<
ScriptInterp.o : ScriptInterp.cpp
	${CXX} ${CXXFLAGS} -Wno-deprecated -I ${IDIR} -I ${SBIDIR} -c $<

.cpp.o:	
	${CXX} ${CXXFLAGS} -I ${IDIR} -I ${SBIDIR} -c $<

all:    
	make ${LIB}

${LIB} : ${OFILES}
	ar -ru ${LIB} ${OFILES}

clean:
	rm -f *.o
