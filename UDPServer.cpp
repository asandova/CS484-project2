/*
*   File: UDPServer.cpp
*   Author: August B. Sandoval
*   Date: 2018-10-19
*   Purpose: Contains the UDPServer class Definition
*   Class: CS484
*/

//Socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <signal.h>

#include "UDPServer.hpp"

using namespace std;

bool UDPServer::DebugMode = false;
bool UDPServer::verboseMode = false;

UDPServer::UDPServer(string filename){
    BufferLength = 512;
    Port = 65535;
    Buffer = string();
    Buffer.resize(BufferLength, '\0');
    ifstream check(filename);
    if(!check.good()){
        cout << "Error: File " << filename << " does not exist!\nTerminating Server" << endl;
        exit(1);
    }
    FileToServer = filename;
    Clients = vector<struct OpenConnections>();
    TimeInterval.tv_usec = 500;

    Ssocket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if( Ssocket == -1 ){
        perror((char*)Ssocket);
        exit(1);
    }
    //bzero( &my_addr, sizeof(my_addr) );
    memset( (char*) &my_addr,0, sizeof(my_addr) );
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(Port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if( bind(Ssocket, (struct sockaddr*)&my_addr, sizeof(my_addr) ) == -1)
    {
        perror("bind");
        exit(1);
    }
}
UDPServer::UDPServer(string filename, int port){
    BufferLength = 512;
    Port = port;
    Buffer = string();
    Buffer.resize(BufferLength, '\0');
    ifstream check(filename);
    if(!check.good()){
        cout << "Error: File " << filename << " does not exist!\nTerminating Server" << endl;
        exit(1);
    }
    FileToServer = filename;
    Clients = vector<struct OpenConnections>();
    TimeInterval.tv_usec = 500;

    Ssocket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if( Ssocket == -1 ){
        perror((char*)Ssocket);
        exit(1);
    }

    memset( (char*) &my_addr,0, sizeof(my_addr) );
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(Port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if( bind(Ssocket, (struct sockaddr*)&my_addr, sizeof(my_addr) ) == -1)
    {
        perror("bind");
        exit(1);
    }
    if(debugMode||verboseMode){
        cout << "Socket " << Ssocket << " was sucsessfully binded" << endl;
    }
    
}
void UDPServer::run(){
    signal(SIGINT, terminateServer);
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(Ssocket, &rfds);
    int running = 1;
    while(running){
        int selRet = select(Ssocket+1, &rfds, NULL,NULL, &TimeInterval);
        if(selRet == -1){
            //error
            perror("bind");
            closeSocket();
            exit(1);
        }
        else if(selRet == 0){
            //timeout
            double duration;
            vector<struct OpenConnections>::iterator itr;
            for(itr = Clients.begin(); itr != Clients.end(); ++itr){
                duration = (clock() - itr->lastSent) / (double) CLOCKS_PER_SEC;
                if(duration > 90){//checks if this connection excessed wait time
                    if(itr->tries < 5){
                        //resend data
                        itr->tries++;
                        Send( UDPData::toUDP( itr->toSend[itr->position] ) , itr->address, itr->Slen );
                        itr->lastSent = clock();
                    }else{
                        //terminate connection after five timeouts
                        Clients.erase(itr);
                    }
                }
            }
        }else{
            Receive();
            struct DataBlock packet;
            bool newConnection = true;
            vector<struct OpenConnections>::iterator itr;
            for(itr = Clients.begin(); itr != Clients.end(); ++itr){
                if(client_addr.sin_addr.s_addr  == itr->address.sin_addr.s_addr ){
                    newConnection = false;
                    packet = UDPData::fromUDP(Buffer, itr->PacketLength);
                    if(packet.Ack){
                        if(packet.index == itr->position){
                            itr->position++;
                            itr->tries = 0;
                        }else{
                            itr->position = packet.index;
                            itr->tries++;
                        }
                        Send( UDPData::toUDP( itr->toSend[itr->position]),itr->address, itr->Slen );
                        itr->lastSent = clock();
                    }else if(packet.handshake){
                        if(packet.Ack){
                            Send( UDPData::toUDP( itr->toSend[itr->position]),itr->address, itr->Slen );
                            itr->lastSent = clock();
                        }else{
                            struct DataBlock resend;
                            resend.index = itr->toSend.size();
                            resend.data = string('\0',itr->PacketLength - 13);
                            resend.Ack = true;
                            resend.handshake = true;
                            resend.terminate = false;
                            Send( UDPData::toUDP(resend) ,itr->address, itr->Slen );
                            itr->lastSent = clock();
                        }
                    }else if(packet.terminate){
                        Send( UDPData::toUDP(packet) ,itr->address , itr->Slen );
                        Clients.erase(itr);
                    }
                    break;
                }
            }
            if(newConnection){
                packet = UDPData::fromUDP(Buffer,BufferLength);
                //add to active list
                struct OpenConnections n;
                n.PacketLength = packet.index;
                n.position = 0;
                n.address = client_addr;
                n.Slen = sizeof(client_addr);
                n.tries = 0;
                n.toSend = UDPData(packet.index);
                n.toSend.parseFile(FileToServer);
                packet.data = string('\0', n.PacketLength - 13);
                packet.index = n.toSend.size();
                packet.Ack = true;
                packet.handshake = true;
                packet.terminate = false;
                Send( UDPData::toUDP(packet),n.address, n.Slen );
            }
        }
        //clear client_addr
    }
}
/*
void UDPServer::echo(){
    /*
        Does a simple echo procedure
    /
    int close = 0;
    int counter = 0;
    while(counter < 1){
        Receive();
        string echo = Buffer;
        Send( echo );
        counter++;
    }
    closeSocket();
}*/

void UDPServer::Receive(){
    /*
        Receives incoming UDP data from socket
    */
    if(DebugMode || verboseMode) {cout << "Receiving..." << endl;}
    char* buf = &Buffer[0];
    memset(buf, '\0', BufferLength);
    if( ( receiveLength = recvfrom(Ssocket, buf, BufferLength, 0, (struct sockaddr *) &client_addr, &Slength ) ) == -1 ){
        perror("recvfrom()");
        close(Ssocket);
        exit(1);
    }
    Buffer = buf;
    if(DebugMode || verboseMode){
        cout << "ReceivedLength: " << receiveLength << endl;
        cout << "Received packet from " << inet_ntoa(client_addr.sin_addr)  << ":" << ntohs(client_addr.sin_port) << endl;
        cout << "Data: " << Buffer << endl;
    }
}

void UDPServer::Send(string data, struct sockaddr_in client, socklen_t Clen){
    /*
        Send data to desired address
    */
   if(DebugMode || verboseMode){
        cout << "Sending..." << endl;
        cout << "Data:" << data << endl;
    }
    int sentlength;
    if ( (sentlength =  sendto(Ssocket, data.c_str() , data.size(), 0, (struct sockaddr*) &client, Clen)) == -1){
        perror("sendto()");
        close(Ssocket);
        exit(1);
    }
    if(DebugMode || verboseMode){cout << "sent Length:" << sentlength << endl;}

}
void UDPServer::closeSocket(){
    /*
        Properly closes the socket
    */
    if(DebugMode || verboseMode){ cout << "closing socket " << Ssocket << endl;}
    if (close(Ssocket) == -1){
        perror("Socket Closing Error: ");
        exit(1);
    }
    if(DebugMode || verboseMode){ cout << "Socket " << Ssocket  << " successfuly closed"<< endl;}
}
void UDPServer::terminateServer( int signum ){
    cout << "Interrupt signal (" << signum << ") received." << endl;

    closeSocket();
    exit(signum);
}