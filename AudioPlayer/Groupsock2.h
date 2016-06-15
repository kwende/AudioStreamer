#pragma once
#include "liveMedia.hh"
#include "GroupsockHelper.hh"
#include <iostream>

class Groupsock2 : public Groupsock
{
public:
    Groupsock2(UsageEnvironment& env, struct in_addr const& groupAddr,
        Port port, u_int8_t ttl) : Groupsock(env, groupAddr, port, ttl)
    {
        _gotAddress = false; 
        return; 
    }
    // used for a 'source-independent multicast' group
    Groupsock2(UsageEnvironment& env, struct in_addr const& groupAddr,
        struct in_addr const& sourceFilterAddr,
        Port port) : Groupsock(env, groupAddr, sourceFilterAddr, port)
    {
        _gotAddress = false;
        return; 
    }

    Boolean Groupsock2::handleRead(unsigned char* buffer, unsigned bufferMaxSize,
        unsigned& bytesRead, struct sockaddr_in& fromAddressAndPort) {
        Boolean result = Groupsock::handleRead(buffer, bufferMaxSize, bytesRead, fromAddressAndPort);
        if (result && !_gotAddress) {
            // “fromAddressAndPort” is the sender’s IP address (and port number); record it
            _address = fromAddressAndPort; 
            _gotAddress = true; 
        }

        return result;
    }
    
    sockaddr_in GetAddress()
    {
        return _address; 
    }

private: 
    sockaddr_in _address; 
    bool _gotAddress; 
};

