CXXFLAGS=-g

all: testrun

testrun: testrun.o hw2.o
	g++ -o testrun testrun.o hw2.o -lpthread

testrun.o: hw2.h testrun.cpp

hw2.o: hw2.cpp hw2.h monitor.h
	g++ $(CXXFLAGS) -c hw2.cpp

clean:
	rm -f testrun.o hw2.o testrun
