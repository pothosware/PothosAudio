// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "AudioBlock.hpp"
#include <algorithm> //min/max
#include <iostream>

/***********************************************************************
 * |PothosDoc Audio Sink
 *
 * The audio sink forwards an input sample stream into an audio output device.
 * In interleaved mode, the samples are interleaved from one input port,
 * In the port-per-channel mode, each audio channel uses a separate port.
 *
 * |category /Audio
 * |category /Sinks
 * |keywords audio sound stereo mono speaker
 *
 * |param deviceName[Device Name] The name of an audio device on the system,
 * the integer index of an audio device on the system,
 * or an empty string to use the default input device.
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
 * |param dtype[Data Type] The data type consumed by the audio sink.
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
 * |param reportMode [Report Mode] Options for reporting underflow.
 * <ul>
 * <li>"LOGGER" - reports the full error message to the logger</li>
 * <li>"STDERROR" - prints "aU" (audio underflow) to stderror</li>
 * <li>"DISABLED" - disabled mode turns off all reporting</li>
 * </ul>
 * |default "STDERROR"
 * |option [Logging Subsystem] "LOGGER"
 * |option [Standard Error] "STDERROR"
 * |option [Reporting Disabled] "DISABLED"
 * |preview disable
 * |tab Underflow
 *
 * |param backoffTime [Backoff Time] Configurable wait for mitigating underflows.
 * The sink block will not consume samples after an underflow for the specified wait time.
 * A small wait time of several milliseconds can help to prevent cascading underflows
 * when the upstream source is not keeping up with the configured audio rate.
 * |units milliseconds
 * |preview valid
 * |default 0
 * |tab Underflow
 *
 * |factory /audio/sink(dtype, numChans, chanMode)
 * |initializer setupDevice(deviceName)
 * |initializer setupStream(sampRate)
 * |setter setReportMode(reportMode)
 * |setter setBackoffTime(backoffTime)
 **********************************************************************/
class AudioSink : public AudioBlock
{
public:
    AudioSink(const Pothos::DType &dtype, const size_t numChans, const std::string &chanMode):
        AudioBlock("AudioSink", true, dtype, numChans, chanMode)
    {
        //setup ports
        if (_interleaved) this->setupInput(0, Pothos::DType::fromDType(dtype, numChans));
        else for (size_t i = 0; i < numChans; i++) this->setupInput(i, dtype);
    }

    static Block *make(const Pothos::DType &dtype, const size_t numChans, const std::string &chanMode)
    {
        return new AudioSink(dtype, numChans, chanMode);
    }

    void work(void)
    {
        if (this->workInfo().minInElements == 0) return;

        //calculate the number of frames
        int numFrames = Pa_GetStreamWriteAvailable(_stream);
        if (numFrames < 0)
        {
            throw Pothos::Exception("AudioSink::work()", "Pa_GetStreamWriteAvailable: " + std::string(Pa_GetErrorText(numFrames)));
        }
        if (numFrames == 0) numFrames = MIN_FRAMES_BLOCKING;
        numFrames = std::min<int>(numFrames, this->workInfo().minInElements);

        //get the buffer
        const void *buffer = nullptr;
        if (_interleaved) buffer = this->workInfo().inputPointers[0];
        else buffer = (const void *)this->workInfo().inputPointers.data();

        //peform write to the device
        PaError err = Pa_WriteStream(_stream, buffer, numFrames);

        //handle the error reporting
        bool logError = err != paNoError;
        if (err == paOutputUnderflowed)
        {
            _readyTime += _backoffTime;
            if (_reportStderror) std::cerr << "aU" << std::flush;
            logError = _reportLogger;
        }
        if (logError)
        {
            poco_error(_logger, "Pa_WriteStream: " + std::string(Pa_GetErrorText(err)));
        }

        //not ready to consume because of backoff
        if (_readyTime >= std::chrono::high_resolution_clock::now()) return this->yield();

        //consume buffer (all modes)
        for (auto port : this->inputs()) port->consume(numFrames);
    }
};

static Pothos::BlockRegistry registerAudioSink(
    "/audio/sink", &AudioSink::make);
