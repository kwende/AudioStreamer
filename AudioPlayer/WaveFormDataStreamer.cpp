#include "stdafx.h"
#include "WaveFormDataStreamer.h"


WaveFormDataStreamer::WaveFormDataStreamer(UsageEnvironment& env, Groupsock* RTPgs,
    unsigned char rtpPayloadFormat,
    unsigned rtpTimestampFrequency) : MultiFramedRTPSource(env, RTPgs, rtpPayloadFormat, rtpTimestampFrequency)
{
}


WaveFormDataStreamer::~WaveFormDataStreamer()
{
}

WaveFormDataStreamer* WaveFormDataStreamer::createNew(UsageEnvironment& env, Groupsock* RTPgs,
    unsigned char rtpPayloadFormat, unsigned int rtpTimestampFrequency)
{
    return new WaveFormDataStreamer(env, RTPgs, rtpPayloadFormat, rtpTimestampFrequency); 
}

char const* WaveFormDataStreamer::MIMEtype() const
{
    return "audio/L16"; 
}