// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <Poco/Logger.h>
#include <portaudio.h>
#include <chrono>

class AudioBlock : public Pothos::Block
{
public:
    AudioBlock(const std::string &blockName, const bool isSink, const Pothos::DType &dtype, const size_t numChans, const std::string &chanMode);
    ~AudioBlock(void);

    std::string getDescOverlay(void) const;

    void setupDevice(const std::string &deviceName);
    void setupStream(const double sampRate);

    void setReportMode(const std::string &mode);
    void setBackoffTime(const long backoff);

    void activate(void);
    void deactivate(void);

protected:
    const std::string _blockName;
    const bool _isSink;
    Poco::Logger &_logger;
    PaStream *_stream;
    PaStreamParameters _streamParams;
    bool _interleaved;
    bool _sendLabel;
    bool _reportLogger;
    bool _reportStderror;
    std::chrono::high_resolution_clock::duration _backoffTime;
    std::chrono::high_resolution_clock::time_point _readyTime;
};
