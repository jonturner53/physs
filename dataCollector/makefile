SHELL := /bin/bash
CLIB := lib-fizz.a
IDIR := include
SBIDIR := include/seabreeze

CXXFLAGS := -Wall -Wno-deprecated -O2 -std=c++0x

${OFILES} : ${HFILES}

.cpp.o:	${IDIR} ${SBIDIR}
	${CXX} ${CXXFLAGS} -I ${IDIR} -I ${SBIDIR} -c $<

collector: collector.o ${CLIB}
	${CXX} ${CXXFLAGS} $< ${CLIB} -l seabreeze -l usb -l pthread -o $@

basicTest: basicTest.o ${CLIB}
	${CXX} ${CXXFLAGS} $< ${CLIB} -l seabreeze -l usb -l pthread -o $@

all: 
	cpufreq-set -u 1000M
	make -C misc all
	make -C hardware all 
	make -C components all	  
	make collector
	make basicTest

clean:
	rm -f lib-fizz.a
	rm -f *.o
	make -C misc clean
	make -C hardware clean
	make -C components clean
