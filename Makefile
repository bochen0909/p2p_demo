CC=gcc
CXX=g++
DEBUGFLAG=-ggdb	#remove this for release
LOG_FLAG=-DLOG_LEVEL=3 -DG04_INFO_STDOUT #-DG04_DEBUG_STDOUT
CFLAGS=-Wall -ansi -pedantic $(DEBUGFLAG) $(LOG_FLAG)
CXXFLAGS=-Wall -ansi -pedantic -std=c++11 $(DEBUGFLAG) $(LOG_FLAG)
LDFLAGS=
EXECUTABLE=g04 

all: $(EXECUTABLE)
    

g04: g04_main.o G04.o  Util.o Listener.o Packet.o
	$(CXX) $(LDFLAGS) $^ -o $@ -lreadline -lpthread

	
tcpservselect01.x: tcpservselect01.c
	$(CC) $(LDFLAGS) $^ -o $@	

.PHONY: test
test:
	make -C test
	
clean:
	rm -fr ${EXECUTABLE} *.o *.x core
	make -C test clean

