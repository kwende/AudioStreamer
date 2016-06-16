
// AudioPlayer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "liveMedia.hh"
#include "GroupsockHelper.hh"
#include "Groupsock2.h"
#include <Windows.h>
#include "BasicUsageEnvironment.hh"
#include "WaveFormDataStreamer.h"
#include <iostream>
#include "SpeakerSink.h"
#include <string>

void afterPlaying(void* clientData); // forward

void playSource()
{
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* environment = BasicUsageEnvironment::createNew(*scheduler);

    SpeakerSink* sink = SpeakerSink::createNew(*environment, "fromclient.audio");

    unsigned int rtpPortNum = 1234;
    unsigned int rtcpPortNum = rtpPortNum + 1;
    char* ipAddress = "0.0.0.0";
    //char* ipAddress = "239.255.42.42"; 

    struct in_addr address;
    address.S_un.S_addr = our_inet_addr(ipAddress);
    const Port rtpPort(rtpPortNum);
    const Port rtcpPort(rtcpPortNum);

    Groupsock2 rtpGroupSock(*environment, address, rtpPort, 1);
    Groupsock2 rtcpGroupSock(*environment, address, rtcpPort, 1);

    sink->SetGroupSocks(&rtpGroupSock, &rtcpGroupSock); 

    //RTPSource* rtpSource = WaveFormDataStreamer::createNew(*environment, &rtpGroupSock);
    int payloadFormatCode = 11;
    const char* mimeType = "L16";
    int fSamplingFrequency = 44100;
    int fNumChannels = 1;
    RTPSource* rtpSource = SimpleRTPSource::createNew(
        *environment, (Groupsock*)&rtpGroupSock, payloadFormatCode,
        fSamplingFrequency, "audio/L16", 0, False /*no 'M' bit*/);

    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen + 1];
    gethostname((char*)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case

    RTCPInstance* rtcpInstance =
        RTCPInstance::createNew(*environment, (Groupsock*)&rtcpGroupSock, 5000, CNAME, NULL, rtpSource);

    *environment << "Beginning receiving multicast stream...\n";

    sink->startPlaying(*rtpSource, afterPlaying, NULL);

    environment->taskScheduler().doEventLoop(); // does not return
}

const int NumberOfHeaders = 20; 
WAVEHDR* waveHeaders; 
int currentHeader = 0; 
int headersInUse = 0; 
CRITICAL_SECTION section; 

#include <fstream>
void CALLBACK waveOutProc2(
    HWAVEOUT  hwo,
    UINT      uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{
    std::cout << "!"; 
    ::EnterCriticalSection(&section); 
    headersInUse--; 
    ::LeaveCriticalSection(&section); 
    return; 
}


void doAudioFromDirectory()
{
    ::InitializeCriticalSection(&section); 

    WAVEFORMATEX pFormat;

    pFormat.nSamplesPerSec = 44100;
    pFormat.wBitsPerSample = 16;
    pFormat.nChannels = 1;
    pFormat.cbSize = 0;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nBlockAlign = (pFormat.wBitsPerSample >> 3) * pFormat.nChannels;
    pFormat.nAvgBytesPerSec = pFormat.nBlockAlign * pFormat.nSamplesPerSec;

    HWAVEOUT waveHandle;

    MMRESULT result = ::waveOutOpen(
        &waveHandle,
        WAVE_MAPPER,
        &pFormat,
        (DWORD_PTR)waveOutProc2,
        (DWORD_PTR)0,
        CALLBACK_FUNCTION);

    std::ofstream out("merged2.audio", std::ios::binary);

    waveHeaders = new WAVEHDR[NumberOfHeaders];

    for (int c = 1; c <= 500; c++)
    {
        std::ifstream in(std::to_string(c) + ".audio", std::ios::binary | std::ios::ate);
        std::streampos pos = in.tellg();
        in.seekg(0, in.beg);
        char* dataCopy = new char[pos];
        in.read(dataCopy, pos);
        in.close();
        out.write(dataCopy, pos);

        while (headersInUse == NumberOfHeaders)
        {
            ::Sleep(10); 
        }

        if (headersInUse < NumberOfHeaders)
        {
            ::EnterCriticalSection(&section);
            headersInUse++;
            ::LeaveCriticalSection(&section);


            std::cout << currentHeader % NumberOfHeaders << ","; 
            WAVEHDR *waveHeader = &waveHeaders[currentHeader % NumberOfHeaders]; 

            if (waveHeader->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(waveHandle, waveHeader, sizeof(WAVEHDR));

            ::ZeroMemory(waveHeader, sizeof(waveHeader));
            waveHeader->dwBufferLength = pos;
            waveHeader->lpData = (LPSTR)dataCopy;

            ::waveOutPrepareHeader(waveHandle, waveHeader, sizeof(WAVEHDR));

            MMRESULT result = ::waveOutWrite(waveHandle, waveHeader, sizeof(WAVEHDR));
            currentHeader++; 
        }


       // while (::waveOutUnprepareHeader(waveHandle, &waveHeader, sizeof(WAVEHDR)) != 0)
        //{
            //::Sleep(5);
        //}
        //std::cout << ".";
    }

    //for (int c = 1; c <= 500;c+=3)
    //{
    //    for (int j = c; j < c + 3; j++)
    //    {
    //        std::ifstream in("c:/users/brush/desktop/output/" + std::to_string(j) + ".audio", std::ios::binary | std::ios::ate);
    //        std::streampos pos = in.tellg();
    //        in.seekg(0, in.beg);
    //        char* dataCopy = new char[pos];
    //        in.read(dataCopy, pos);
    //        in.close();
    //        out.write(dataCopy, pos);

    //        WAVEHDR waveHeader;
    //        ::ZeroMemory(&waveHeader, sizeof(waveHeader));
    //        waveHeader.dwBufferLength = pos;
    //        waveHeader.lpData = (LPSTR)dataCopy;

    //        ::waveOutPrepareHeader(waveHandle, &waveHeader, sizeof(WAVEHDR));
    //        ::waveOutWrite(waveHandle, &waveHeader, sizeof(WAVEHDR));

    //        // while (::waveOutUnprepareHeader(waveHandle, &waveHeader, sizeof(WAVEHDR)) != 0)
    //        //{
    //        //}
    //        std::cout << ".";
    //    }
    //    ::Sleep(30);

    //}

    out.close();

    getchar();

    //::waveOutUnprepareHeader(waveHandle, &waveHeader, sizeof(WAVEHDR));
}

void doAudio()
{
    WAVEFORMATEX pFormat;

    pFormat.nSamplesPerSec = 44100;
    pFormat.wBitsPerSample = 16;
    pFormat.nChannels = 1;
    pFormat.cbSize = 0;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;

    pFormat.nBlockAlign = (pFormat.wBitsPerSample >> 3) * pFormat.nChannels;
    pFormat.nAvgBytesPerSec = pFormat.nBlockAlign * pFormat.nSamplesPerSec;

    HWAVEOUT waveHandle;

    MMRESULT result = ::waveOutOpen(
        &waveHandle,
        WAVE_MAPPER,
        &pFormat,
        0,
        0,
        CALLBACK_NULL);

    std::ifstream in("merged2.audio", std::ios::binary | std::ios::ate);
    std::streampos pos = in.tellg();
    in.seekg(0, in.beg);
    char* dataCopy = new char[pos];
    in.read(dataCopy, pos);

    WAVEHDR waveHeader;
    ::ZeroMemory(&waveHeader, sizeof(waveHeader));
    waveHeader.dwBufferLength = pos;
    waveHeader.lpData = (LPSTR)dataCopy;

    ::waveOutPrepareHeader(waveHandle, &waveHeader, sizeof(WAVEHDR));
    ::waveOutWrite(waveHandle, &waveHeader, sizeof(WAVEHDR));
    ::waveOutUnprepareHeader(waveHandle, &waveHeader, sizeof(WAVEHDR));

    getchar();
}

void doServer()
{
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    BasicUsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

    unsigned int rtpPortNumber = 1234;
    unsigned int rtcpPortNumber = rtpPortNumber + 1;
    unsigned int ttl = 7;

    struct in_addr destinationAddress;
    //destinationAddress.s_addr = our_inet_addr("172.17.5.92");
    //destinationAddress.s_addr = our_inet_addr("172.17.5.156"); //me
    destinationAddress.s_addr = our_inet_addr("172.17.5.26"); // lucas
    //destinationAddress.s_addr = our_inet_addr("239.255.42.42");
    const Port rtpPort(rtpPortNumber);
    const Port rtcpPort(rtcpPortNumber);

    Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
    Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);

    int payloadFormatCode = 11;
    const char* mimeType = "L16";
    int fSamplingFrequency = 44100;
    int fNumChannels = 1;

    //return SimpleRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, );
    SimpleRTPSink* sink = SimpleRTPSink::createNew(*env, &rtpGroupsock,
        payloadFormatCode, fSamplingFrequency,
        "audio", mimeType, fNumChannels);

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen + 1];
    gethostname((char*)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case

    RTCPInstance::createNew(*env, &rtcpGroupsock,
        estimatedSessionBandwidth, CNAME,
        sink, NULL /* we're a server */, False);

    unsigned char bitsPerSample = 16;
    unsigned char numChannels = 1;
    unsigned samplingFrequency = 44100;
    unsigned granularityInMS = 20;

    AudioInputDevice *source = AudioInputDevice::createNew(*env, 0, bitsPerSample,
        numChannels, samplingFrequency);
    FramedSource* swappedSource = EndianSwap16::createNew(*env, source);

    Boolean started = sink->startPlaying(*swappedSource, afterPlaying, sink);

    env->taskScheduler().doEventLoop();

}

void doWavServer()
{
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    BasicUsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

    unsigned int rtpPortNumber = 1234;
    unsigned int rtcpPortNumber = rtpPortNumber + 1;
    unsigned int ttl = 7;

    struct in_addr destinationAddress;
    //destinationAddress.s_addr = our_inet_addr("172.17.5.92");
    destinationAddress.s_addr = our_inet_addr("172.17.5.156");
    //destinationAddress.s_addr = our_inet_addr("239.255.42.42");
    const Port rtpPort(rtpPortNumber);
    const Port rtcpPort(rtcpPortNumber);

    Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
    Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);

    int payloadFormatCode = 11;
    const char* mimeType = "L16";
    int fSamplingFrequency = 44100;
    int fNumChannels = 1;

    //return SimpleRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, );
    SimpleRTPSink* sink = SimpleRTPSink::createNew(*env, &rtpGroupsock,
        payloadFormatCode, fSamplingFrequency,
        "audio", mimeType, fNumChannels);

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen + 1];
    gethostname((char*)CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case

    RTCPInstance::createNew(*env, &rtcpGroupsock,
        estimatedSessionBandwidth, CNAME,
        sink, NULL /* we're a server */, False);

    unsigned char bitsPerSample = 16;
    unsigned char numChannels = 1;
    unsigned samplingFrequency = 44100;
    unsigned granularityInMS = 20;

    WAVAudioFileSource* waveAudioSource = WAVAudioFileSource::createNew(*env, "C:/users/brush/desktop/music.wav");
    //AudioInputDevice *audioInputSource = AudioInputDevice::createNew(this->envir(), 0, bitsPerSample,
    //    numChannels, samplingFrequency);
    FramedSource* swappedSource = EndianSwap16::createNew(*env, waveAudioSource);

    Boolean started = sink->startPlaying(*swappedSource, afterPlaying, sink);

    env->taskScheduler().doEventLoop();

}

int main(int argc, char** argv)
{
    //doAudioFromDirectory();
    //return 0;
    if (argc > 0 && strcmp(argv[1], "s") == 0)
    {
        std::cout << "running as server" << std::endl;
        doServer();
        return 0;
    }
    else
    {
        std::cout << "running as client" << std::endl;
        playSource();
        return 0;
    }

    return 0;
}

void afterPlaying(void* /*clientData*/) {

    // End by closing the media:
    return;
}