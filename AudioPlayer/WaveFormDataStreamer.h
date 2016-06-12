#pragma once

#include "MultiFramedRTPSource.hh"

class WaveFormDataStreamer : public MultiFramedRTPSource
{
public:
    static WaveFormDataStreamer* createNew(UsageEnvironment& env, Groupsock* RTPgs,
        unsigned char rtpPayloadFormat = 14,
        unsigned rtpTimestampFrequency = 90000); 
protected:
    virtual ~WaveFormDataStreamer(); 
private:
    WaveFormDataStreamer(UsageEnvironment& env, Groupsock* RTPgs,
        unsigned char rtpPayloadFormat,
        unsigned int rtpTimestampFrequence); 
    virtual char const* MIMEtype() const;
};

