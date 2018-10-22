all: serverDriver clientDriver

clientDriver: cdriver client datablock
	g++ clientdriver.o UDPClient.o -o client

cdriver: clientdriver.cpp
	g++ -c clientdriver.cpp

client: UDPClient.cpp UDPClient.hpp
	g++ -c UDPClient.cpp


serverDriver: sdriver server datablock
	g++ serverdriver.o UDPServer.o -o server

sdriver: serverdriver.cpp
	g++ -c serverdriver.cpp

server: UDPServer.hpp UDPServer.cpp
	g++ -c UDPServer.cpp

datablock: UDPDataBlock.hpp
	g++ -c UDPDataBlock.cpp

clean:
	rm -rf *.o
	rm server
	rm client