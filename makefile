# makefile

all: dataserver client

common.o: common.h common.cpp
	g++ -g -w -std=c++14 -c common.cpp

Histogram.o: Histogram.h Histogram.cpp
	g++ -g -w -std=c++14 -c Histogram.cpp

FIFOreqchannel.o: FIFOreqchannel.h FIFOreqchannel.cpp
	g++ -g -w -std=c++14 -c FIFOreqchannel.cpp

MessageQueueChannel.o: MessageQueueChannel.h MessageQueueChannel.cpp
	g++ -g -w -std=c++14 -c MessageQueueChannel.cpp
	
SHMChannel.o: SHMChannel.h SHMChannel.cpp
	g++ -g -w -std=c++14 -c SHMChannel.cpp

client: client.cpp Histogram.o FIFOreqchannel.o MessageQueueChannel.o SHMChannel.o common.o
	g++ -g -w -std=c++14 -o client client.cpp Histogram.o FIFOreqchannel.o MessageQueueChannel.o SHMChannel.o common.o -lpthread -lrt

dataserver: dataserver.cpp  FIFOreqchannel.o MessageQueueChannel.o SHMChannel.o common.o
	g++ -g -w -std=c++14 -o dataserver dataserver.cpp FIFOreqchannel.o MessageQueueChannel.o SHMChannel.o common.o -lpthread -lrt

clean:
	rm -rf *.o fifo* dataserver client /dev/mqueue/* /dev/shm/*