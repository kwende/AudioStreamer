
#include "stdafx.h"
#include "SpeakerSink.h"
#include "FileSink.hh"
#include "GroupsockHelper.hh"
#include "OutputFile.hh"
#include <Windows.h>
#include <string>
#include <fstream>
#include <iostream>
#include "Groupsock2.h"

const int NumberOfHeaders = 20;

#include <fstream>

void CALLBACK waveOutProc(
    HWAVEOUT  hwo,
    UINT      uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{
    SpeakerSink* speakerSink = (SpeakerSink*)dwInstance; 
    speakerSink->HandleCallback(); 

    return;
}

void SpeakerSink::HandleCallback()
{
    ::EnterCriticalSection(&section);
    headersInUse--;
    ::LeaveCriticalSection(&section);
}

SpeakerSink::SpeakerSink(UsageEnvironment& env, FILE* fid, unsigned bufferSize,
    char const* perFrameFileNamePrefix, bool openBackChannel)
    : MediaSink(env), fOutFid(fid), fBufferSize(bufferSize), fSamePresentationTimeCounter(0) {

    _openBackChannel = openBackChannel; 

    fBuffer = new unsigned char[bufferSize];
    if (perFrameFileNamePrefix != NULL) {
        fPerFrameFileNamePrefix = strDup(perFrameFileNamePrefix);
        fPerFrameFileNameBuffer = new char[strlen(perFrameFileNamePrefix) + 100];
    }
    else {
        fPerFrameFileNamePrefix = NULL;
        fPerFrameFileNameBuffer = NULL;
    }
    fPrevPresentationTime.tv_sec = ~0; fPrevPresentationTime.tv_usec = 0;

    WAVEFORMATEX pFormat;

    pFormat.nSamplesPerSec = 44100;
    pFormat.wBitsPerSample = 16;
    pFormat.nChannels = 1;
    pFormat.cbSize = 0;
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nBlockAlign = (pFormat.wBitsPerSample >> 3) * pFormat.nChannels;
    pFormat.nAvgBytesPerSec = pFormat.nBlockAlign * pFormat.nSamplesPerSec;

    ::InitializeCriticalSection(&section); 

    MMRESULT result = ::waveOutOpen(
        &waveHandle,
        WAVE_MAPPER,
        &pFormat,
        (DWORD_PTR)waveOutProc,
        (DWORD_PTR)this,
        CALLBACK_FUNCTION);

    waveHeaders = new WAVEHDR[NumberOfHeaders];
    for (int c = 0; c < NumberOfHeaders; c++)
    {
        ::ZeroMemory(&waveHeaders[c], sizeof(WAVEHDR)); 
    }
    _sentBack = false; 
}

SpeakerSink::~SpeakerSink() {
    delete[] fPerFrameFileNameBuffer;
    delete[] fPerFrameFileNamePrefix;
    delete[] fBuffer;
    if (fOutFid != NULL) fclose(fOutFid);
}

SpeakerSink* SpeakerSink::createNew(UsageEnvironment& env, char const* fileName, Boolean openBackChannel,
    unsigned bufferSize, Boolean oneFilePerFrame) {

    do {
        FILE* fid;
        char const* perFrameFileNamePrefix;
        if (oneFilePerFrame) {
            // Create the fid for each frame
            fid = NULL;
            perFrameFileNamePrefix = fileName;
        }
        else {
            // Normal case: create the fid once
            fid = OpenOutputFile(env, fileName);
            if (fid == NULL) break;
            perFrameFileNamePrefix = NULL;
        }

        return new SpeakerSink(env, fid, bufferSize, perFrameFileNamePrefix, openBackChannel);
    } while (0);

    return NULL;
}

Boolean SpeakerSink::continuePlaying() {
    if (fSource == NULL) return False;

    fSource->getNextFrame(fBuffer, fBufferSize,
        afterGettingFrame, this,
        onSourceClosure, this);

    return True;
}

void SpeakerSink::afterGettingFrame(void* clientData, unsigned frameSize,
    unsigned numTruncatedBytes,
    struct timeval presentationTime,
    unsigned /*durationInMicroseconds*/) {
    SpeakerSink* sink = (SpeakerSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime);
}

void SpeakerSink::SetGroupSocks(Groupsock2 *rtpGroupSock, Groupsock2 *rtcpGroupSock)
{
    _rtpSock = rtpGroupSock; 
    _rtcpSock = rtcpGroupSock; 
}

int counter = 0; 

void SpeakerSink::addData(unsigned char const* data, unsigned dataSize,
    struct timeval presentationTime) {
    if (fPerFrameFileNameBuffer != NULL && fOutFid == NULL) {
        // Special case: Open a new file on-the-fly for this frame
        if (presentationTime.tv_usec == fPrevPresentationTime.tv_usec &&
            presentationTime.tv_sec == fPrevPresentationTime.tv_sec) {
            // The presentation time is unchanged from the previous frame, so we add a 'counter'
            // suffix to the file name, to distinguish them:
            sprintf(fPerFrameFileNameBuffer, "%s-%lu.%06lu-%u", fPerFrameFileNamePrefix,
                presentationTime.tv_sec, presentationTime.tv_usec, ++fSamePresentationTimeCounter);
        }
        else {
            sprintf(fPerFrameFileNameBuffer, "%s-%lu.%06lu", fPerFrameFileNamePrefix,
                presentationTime.tv_sec, presentationTime.tv_usec);
            fPrevPresentationTime = presentationTime; // for next time
            fSamePresentationTimeCounter = 0; // for next time
        }
        fOutFid = OpenOutputFile(envir(), fPerFrameFileNameBuffer);
    }

    // Write to our file:
#ifdef TEST_LOSS
    static unsigned const framesPerPacket = 10;
    static unsigned const frameCount = 0;
    static Boolean const packetIsLost;
    if ((frameCount++) % framesPerPacket == 0) {
        packetIsLost = (our_random() % 10 == 0); // simulate 10% packet loss #####
    }

    if (!packetIsLost)
#endif

        if (fOutFid != NULL && data != NULL) {

            if (!_sentBack && _openBackChannel)
            {
                RTPSource* source = (RTPSource*)this->fSource;
                sockaddr_in address = ((Groupsock2*)source->RTPgs())->GetAddress();

                char szBuffer[512];
                ::inet_ntop(address.sin_family, &address.sin_addr, szBuffer, sizeof(szBuffer));
                std::cout << szBuffer << std::endl;

                _rtpSock->changeDestinationParameters(address.sin_addr, 5555, 1);
                _rtcpSock->changeDestinationParameters(address.sin_addr, 5556, 1);

                int payloadFormatCode = 11;
                const char* mimeType = "L16";
                int fSamplingFrequency = 44100;
                int fNumChannels = 1;

                Groupsock* rtpGroupsock = source->RTPgs();

                SimpleRTPSink* sink = SimpleRTPSink::createNew(this->envir(), rtpGroupsock,
                    payloadFormatCode, fSamplingFrequency,
                    "audio", mimeType, fNumChannels);

                // Create (and start) a 'RTCP instance' for this RTP sink:
                const unsigned estimatedSessionBandwidth = 5000; // in kbps; for RTCP b/w share
                const unsigned maxCNAMElen = 100;
                unsigned char CNAME[maxCNAMElen + 1];
                gethostname((char*)CNAME, maxCNAMElen);
                CNAME[maxCNAMElen] = '\0'; // just in case

                RTCPInstance::createNew(this->envir(), _rtcpSock,
                    estimatedSessionBandwidth, CNAME,
                    sink, NULL /* we're a server */, False);

                unsigned char bitsPerSample = 16;
                unsigned char numChannels = 1;
                unsigned samplingFrequency = 44100;
                unsigned granularityInMS = 20;

                //WAVAudioFileSource* waveAudioSource = WAVAudioFileSource::createNew(this->envir(), "C:/users/brush/desktop/music.wav"); 

                ////AudioInputDevice *audioInputSource = AudioInputDevice::createNew(this->envir(), 0, bitsPerSample,
                ////    numChannels, samplingFrequency);
                //FramedSource* swappedSource = EndianSwap16::createNew(this->envir(), waveAudioSource);

                AudioInputDevice *audioSource = AudioInputDevice::createNew(this->envir(), 0, bitsPerSample,
                    numChannels, samplingFrequency);
                FramedSource* swappedSource = EndianSwap16::createNew(this->envir(), audioSource);

                Boolean started = sink->startPlaying(*swappedSource, nullptr, sink);

                _sentBack = true; 

                std::cout << "running on port " << address.sin_port << std::endl; 
            }

  /*          std::cout << inet_ntoa(address) << std::endl; */

            while (headersInUse == NumberOfHeaders)
            {
                ::Sleep(10);
            }

            //std::cout << "."; 

            if (headersInUse < NumberOfHeaders)
            {
                ::EnterCriticalSection(&section);
                headersInUse++;
                ::LeaveCriticalSection(&section);

                char* dataCopy = new char[dataSize];
                for (int c = 0; c < dataSize - 1; c += 2)
                {
                    dataCopy[c] = data[c + 1];
                    dataCopy[c + 1] = data[c];
                }

                //std::cout << currentHeader % NumberOfHeaders << ",";
                WAVEHDR *waveHeader = &waveHeaders[currentHeader % NumberOfHeaders];

                if (waveHeader->dwFlags & WHDR_PREPARED)
                    waveOutUnprepareHeader(waveHandle, waveHeader, sizeof(WAVEHDR));

                if (waveHeader->lpData)
                    delete waveHeader->lpData; 

                ::ZeroMemory(waveHeader, sizeof(waveHeader));
                waveHeader->dwBufferLength = dataSize;
                waveHeader->lpData = (LPSTR)dataCopy;

                ::waveOutPrepareHeader(waveHandle, waveHeader, sizeof(WAVEHDR));

                MMRESULT result = ::waveOutWrite(waveHandle, waveHeader, sizeof(WAVEHDR));
                std::cout << result << " "; 
                currentHeader++;
            }

          /*  counter++; 
            std::ofstream fout("c:/users/brush/desktop/output/" + std::to_string(counter) + ".audio", 
                std::ios::binary | std::ios::app); 

                char* dataCopy = new char[dataSize];
                for (int c = 0; c < dataSize-1; c += 2)
                {
                dataCopy[c] = data[c + 1];
                dataCopy[c + 1] = data[c];
                }

            fout.write(dataCopy, dataSize); 
            fout.close(); 

            fwrite(dataCopy, 1, dataSize, fOutFid);
            delete dataCopy;*/

            //char* dataCopy = new char[dataSize];
            //for (int c = 0; c < dataSize - 1; c += 2)
            //{
            //    dataCopy[c] = data[c + 1];
            //    dataCopy[c + 1] = data[c];
            //}

            //WAVEHDR* waveHeader = new WAVEHDR();
            //::ZeroMemory(waveHeader, sizeof(WAVEHDR));
            //waveHeader->dwBufferLength = dataSize;
            //waveHeader->lpData = (LPSTR)dataCopy;

            //::waveOutPrepareHeader(waveHandle, waveHeader, sizeof(WAVEHDR));
            //::waveOutWrite(waveHandle, waveHeader, sizeof(WAVEHDR));
        }
}

void SpeakerSink::afterGettingFrame(unsigned frameSize,
    unsigned numTruncatedBytes,
    struct timeval presentationTime) {
    if (numTruncatedBytes > 0) {
        envir() << "FileSink::afterGettingFrame(): The input frame data was too large for our buffer size ("
            << fBufferSize << ").  "
            << numTruncatedBytes << " bytes of trailing data was dropped!  Correct this by increasing the \"bufferSize\" parameter in the \"createNew()\" call to at least "
            << fBufferSize + numTruncatedBytes << "\n";
    }
    addData(fBuffer, frameSize, presentationTime);

    if (fOutFid == NULL || fflush(fOutFid) == EOF) {
        // The output file has closed.  Handle this the same way as if the input source had closed:
        if (fSource != NULL) fSource->stopGettingFrames();
        onSourceClosure();
        return;
    }

    if (fPerFrameFileNameBuffer != NULL) {
        if (fOutFid != NULL) { fclose(fOutFid); fOutFid = NULL; }
    }

    // Then try getting the next frame:
    continuePlaying();
}