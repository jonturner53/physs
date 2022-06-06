SHELL := /bin/bash
IDIR := ../include
CXXFLAGS := -Wall -O2 -std=c++0x

LIB := ../lib-fizz.a

HFILES = ${IDIR}/Logger.h ${IDIR}/Socket.h \
	${IDIR}/SocketAddress.h ${IDIR}/StreamSocket.h ${IDIR}/Util.h \
	${IDIR}/stdinc.h
OFILES = Logger.o Socket.o SocketAddress.o StreamSocket.o \
	Util.o

${OFILES} : ${HFILES}

.cpp.o:
	${CXX} ${CXXFLAGS} -I ${IDIR} -c $<

all: ${LIB}

cloud: ${LIB}

${LIB} : ${OFILES}
	ar -ru ${LIB} ${OFILES}

clean:
	rm -f *.o
