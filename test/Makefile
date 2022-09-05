CC=gcc
CXX=g++
DEBUGFLAG=-ggdb	#remove this for release
LOG_FLAG=-DLOG_LEVEL=3 -DG04_INFO_STDOUT -DG04_DEBUG_STDOUT
CFLAGS=-Wall -ansi -pedantic $(DEBUGFLAG) $(LOG_FLAG)
CXXFLAGS=-Wall -ansi -pedantic -std=c++11 $(DEBUGFLAG) $(LOG_FLAG) -I$(HOME)/local/include -I..
LDFLAGS=-L$(HOME)/local/lib
EXECUTABLE=gtest.x

all: $(EXECUTABLE)
    

gtest.x: test_main.o TestPacket.o TestUtil.o TestListener.o ../Packet.o ../Util.o ../Listener.o
	$(CXX) $(LDFLAGS) $^ -o $@ -lgtest -lpthread 


clean:
	rm -fr ${EXECUTABLE} *.o *.x core
