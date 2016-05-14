// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "AudioBlock.hpp"
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <cctype>
#include <algorithm>
#include <sstream>

AudioBlock::AudioBlock(const std::string &blockName, const bool isSink, const Pothos::DType &dtype, const size_t numChans, const std::string &chanMode):
    _blockName(blockName),
    _isSink(isSink),
    _logger(Poco::Logger::get(blockName)),
    _stream(nullptr),
    _interleaved(chanMode == "INTERLEAVED"),
    _sendLabel(false),
    _reportLogger(false),
    _reportStderror(true)
{
    this->registerCall(this, POTHOS_FCN_TUPLE(AudioBlock, getDescOverlay));
    this->registerCall(this, POTHOS_FCN_TUPLE(AudioBlock, setupDevice));
    this->registerCall(this, POTHOS_FCN_TUPLE(AudioBlock, setupStream));
    this->registerCall(this, POTHOS_FCN_TUPLE(AudioBlock, setReportMode));
    this->registerCall(this, POTHOS_FCN_TUPLE(AudioBlock, setBackoffTime));

    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        throw Pothos::Exception("AudioBlock()", "Pa_Initialize: " + std::string(Pa_GetErrorText(err)));
    }

    //stream params
    _streamParams.channelCount = numChans;
    if (dtype == Pothos::DType("float32")) _streamParams.sampleFormat = paFloat32;
    if (dtype == Pothos::DType("int32")) _streamParams.sampleFormat = paInt32;
    if (dtype == Pothos::DType("int16")) _streamParams.sampleFormat = paInt16;
    if (dtype == Pothos::DType("int8")) _streamParams.sampleFormat = paInt8;
    if (dtype == Pothos::DType("uint8")) _streamParams.sampleFormat = paUInt8;
    if (not _interleaved) _streamParams.sampleFormat |= paNonInterleaved;
}

AudioBlock::~AudioBlock(void)
{
    if (_stream != nullptr)
    {
        PaError err = Pa_CloseStream(_stream);
        if (err != paNoError)
        {
            poco_error_f1(_logger, "Pa_CloseStream: %s", std::string(Pa_GetErrorText(err)));
        }
    }

    PaError err = Pa_Terminate();
    if (err != paNoError)
    {
        poco_error_f1(_logger, "Pa_Terminate: %s", std::string(Pa_GetErrorText(err)));
    }
}

std::string AudioBlock::getDescOverlay(void) const
{
    Poco::JSON::Object::Ptr topObj(new Poco::JSON::Object());

    Poco::JSON::Array::Ptr params(new Poco::JSON::Array());
    topObj->set("params", params);

    Poco::JSON::Object::Ptr deviceNameParam(new Poco::JSON::Object());
    params->add(deviceNameParam);

    Poco::JSON::Array::Ptr options(new Poco::JSON::Array());
    deviceNameParam->set("options", options);

    for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); i++)
    {
        Poco::JSON::Object::Ptr option(new Poco::JSON::Object());
        const std::string deviceName(Pa_GetDeviceInfo(i)->name);
        option->set("name", deviceName);
        option->set("value", "\""+deviceName+"\"");
    }

    std::stringstream ss;
    topObj->stringify(ss);
    return ss.str();
}

void AudioBlock::setupDevice(const std::string &deviceName)
{
    //empty name, use default
    if (deviceName.empty())
    {
        if (_isSink) _streamParams.device = Pa_GetDefaultOutputDevice();
        else         _streamParams.device = Pa_GetDefaultInputDevice();
        return;
    }

    //numeric name, use index
    if (std::all_of(deviceName.begin(), deviceName.end(), ::isdigit))
    {
        _streamParams.device = std::stoi(deviceName);
        if (_streamParams.device >= Pa_GetDeviceCount()) throw Pothos::RangeException(
            "AudioBlock::setupDevice("+deviceName+")", "Device index out of range");
        return;
    }

    //find the match by name
    for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); i++)
    {
        if (Pa_GetDeviceInfo(i)->name == deviceName)
        {
            _streamParams.device = i;
            return;
        }
    }

    //cant locate by name
    throw Pothos::NotFoundException("AudioBlock::setupDevice("+deviceName+")", "No matching device");
}

void AudioBlock::setupStream(const double sampRate)
{
    //get device info
    const auto deviceInfo = Pa_GetDeviceInfo(_streamParams.device);
    poco_information_f2(_logger, "Using %s through %s",
        std::string(deviceInfo->name), std::string(Pa_GetHostApiInfo(deviceInfo->hostApi)->name));

    //stream params
    if (_isSink) _streamParams.suggestedLatency = (deviceInfo->defaultLowOutputLatency + deviceInfo->defaultHighOutputLatency)/2;
    else         _streamParams.suggestedLatency = (deviceInfo->defaultLowInputLatency + deviceInfo->defaultHighInputLatency)/2;
    _streamParams.hostApiSpecificStreamInfo = nullptr;
    const int requestedSize = Pa_GetSampleSize(_streamParams.sampleFormat);

    //try stream
    PaError err = Pa_IsFormatSupported(nullptr, &_streamParams, sampRate);
    if (err != paNoError)
    {
        throw Pothos::Exception("AudioBlock::setupStream()", "Pa_IsFormatSupported: " + std::string(Pa_GetErrorText(err)));
    }

    //open stream
    err = Pa_OpenStream(
        &_stream, // stream
        _isSink?nullptr:&_streamParams, // inputParameters
        _isSink?&_streamParams:nullptr, // outputParameters
        sampRate,  //sampleRate
        paFramesPerBufferUnspecified, // framesPerBuffer
        0, // streamFlags
        nullptr, //streamCallback
        nullptr); //userData
    if (err != paNoError)
    {
        throw Pothos::Exception("AudioBlock::setupStream()", "Pa_OpenStream: " + std::string(Pa_GetErrorText(err)));
    }
    if (Pa_GetSampleSize(_streamParams.sampleFormat) != requestedSize)
    {
        throw Pothos::Exception("AudioBlock::setupStream()", "Pa_GetSampleSize mismatch");
    }
}

void AudioBlock::setReportMode(const std::string &mode)
{
    if (mode == "LOGGER"){}
    else if (mode == "STDERROR"){}
    else if (mode == "DISABLED"){}
    else throw Pothos::InvalidArgumentException(
        "AudioBlock::setReportMode("+mode+")", "unknown report mode");
    _reportLogger = (mode == "LOGGER");
    _reportStderror = (mode == "STDERROR");
}

void AudioBlock::setBackoffTime(const long backoff)
{
    _backoffTime = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::milliseconds(backoff));
}

void AudioBlock::activate(void)
{
    _readyTime = std::chrono::high_resolution_clock::now();
    PaError err = Pa_StartStream(_stream);
    if (err != paNoError)
    {
        throw Pothos::Exception("AudioBlock::activate()", "Pa_StartStream: " + std::string(Pa_GetErrorText(err)));
    }
    _sendLabel = true;
}

void AudioBlock::deactivate(void)
{
    PaError err = Pa_StopStream(_stream);
    if (err != paNoError)
    {
        throw Pothos::Exception("AudioBlock::deactivate()", "Pa_StopStream: " + std::string(Pa_GetErrorText(err)));
    }
}
