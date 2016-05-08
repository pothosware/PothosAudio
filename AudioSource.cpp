// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "AudioBlock.hpp"
#include <algorithm> //min/max
#include <iostream>

/***********************************************************************
 * |PothosDoc Audio Source
 *
 * The audio source forwards an audio input device to an output sample stream.
 * In interleaved mode, the samples are interleaved into one output port,
 * In the port-per-channel mode, each audio channel uses a separate port.
 *
 * The audio source will post a sample rate stream label named "rxRate"
 * on the first call to work() after activate() has been called.
 * Downstream blocks like the plotter widgets can consume this label
 * and use it to set internal parameters like the axis scaling.
 *
 * |category /Audio
 * |category /Sources
 * |keywords audio sound stereo mono microphone
 *
 * |param deviceName[Device Name] The name of an audio device on the system,
 * the integer index of an audio device on the system,
 * or an empty string to use the default output device.
 * |widget StringEntry()
 * |default ""
 * |preview valid
 *
 * |param sampRate[Sample Rate] The rate of audio samples.
 * |option 32e3
 * |option 44.1e3
 * |option 48e3
 * |default 44.1e3
 * |units Sps
 * |widget ComboBox(editable=true)
 *
 * |param dtype[Data Type] The data type produced by the audio source.
 * |option [Float32] "float32"
 * |option [Int32] "int32"
 * |option [Int16] "int16"
 * |option [Int8] "int8"
 * |option [UInt8] "uint8"
 * |default "float32"
 * |preview disable
 *
 * |param numChans [Num Channels] The number of audio channels.
 * This parameter controls the number of samples per stream element.
 * |widget SpinBox(minimum=1)
 * |default 1
 *
 * |param chanMode [Channel Mode] The channel mode.
 * One port with interleaved channels or one port per channel?
 * |option [Interleaved channels] "INTERLEAVED"
 * |option [One port per channel] "PORTPERCHAN"
 * |default "INTERLEAVED"
 * |preview disable
 *
 * |param reportMode [Report Mode] Options for reporting overflow.
 * <ul>
 * <li>"LOGGER" - reports the full error message to the logger</li>
 * <li>"STDERROR" - prints "aO" (audio overflow) to stderror</li>
 * <li>"DISABLED" - disabled mode turns off all reporting</li>
 * </ul>
 * |default "STDERROR"
 * |option [Logging Subsystem] "LOGGER"
 * |option [Standard Error] "STDERROR"
 * |option [Reporting Disabled] "DISABLED"
 * |preview disable
 * |tab Overflow
 *
 * |param backoffTime [Backoff Time] Configurable wait for mitigating overflows.
 * The source block will not produce samples after an overflow for the specified wait time.
 * A small wait time of several milliseconds can help to prevent cascading overflows
 * when the downstream source is not keeping up with the configured audio rate.
 * |units milliseconds
 * |preview valid
 * |default 0
 * |tab Overflow
 *
 * |factory /audio/source(dtype, numChans, chanMode)
 * |initializer setupDevice(deviceName)
 * |initializer setupStream(sampRate)
 * |setter setReportMode(reportMode)
 * |setter setBackoffTime(backoffTime)
 **********************************************************************/
class AudioSource : public AudioBlock
{
public:
    AudioSource(const Pothos::DType &dtype, const size_t numChans, const std::string &chanMode):
        AudioBlock("AudioSource", false, dtype, numChans, chanMode)
    {
        //setup ports
        if (_interleaved) this->setupOutput(0, Pothos::DType::fromDType(dtype, numChans));
        else for (size_t i = 0; i < numChans; i++) this->setupOutput(i, dtype);
    }

    static Block *make(const Pothos::DType &dtype, const size_t numChans, const std::string &chanMode)
    {
        return new AudioSource(dtype, numChans, chanMode);
    }

    void work(void)
    {
        if (this->workInfo().minOutElements == 0) return;

        //calculate the number of frames
        int numFrames = Pa_GetStreamReadAvailable(_stream);
        if (numFrames < 0)
        {
            throw Pothos::Exception("AudioSource::work()", "Pa_GetStreamReadAvailable: " + std::string(Pa_GetErrorText(numFrames)));
        }
        if (numFrames == 0) numFrames = MIN_FRAMES_BLOCKING;
        numFrames = std::min<int>(numFrames, this->workInfo().minOutElements);

        //get the buffer
        void *buffer = nullptr;
        if (_interleaved) buffer = this->workInfo().outputPointers[0];
        else buffer = (void *)this->workInfo().outputPointers.data();

        //peform read from the device
        PaError err = Pa_ReadStream(_stream, buffer, numFrames);

        //handle the error reporting
        bool logError = err != paNoError;
        if (err == paInputOverflowed)
        {
            _readyTime += _backoffTime;
            if (_reportStderror) std::cerr << "aO" << std::flush;
            logError = _reportLogger;
        }
        if (logError)
        {
            poco_error(_logger, "Pa_ReadStream: " + std::string(Pa_GetErrorText(err)));
        }

        if (_sendLabel)
        {
            _sendLabel = false;
            const auto rate = Pa_GetStreamInfo(_stream)->sampleRate;
            Pothos::Label label("rxRate", rate, 0);
            for (auto port : this->outputs()) port->postLabel(label);
        }

        //not ready to produce because of backoff
        if (_readyTime >= std::chrono::high_resolution_clock::now()) return this->yield();

        //produce buffer (all modes)
        for (auto port : this->outputs()) port->produce(numFrames);
    }
};

static Pothos::BlockRegistry registerAudioSource(
    "/audio/source", &AudioSource::make);
