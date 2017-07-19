// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <yaml-cpp/yaml.h>
#include <portaudio.h>
#include <sstream>

static std::string enumerateAudioDevices(void)
{
    YAML::Node topObject;
    Pa_Initialize();

    YAML::Node devicesArray;
    topObject["PortAudio Device"] = devicesArray;
    for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); i++)
    {
        auto info = Pa_GetDeviceInfo(i);
        YAML::Node infoObject;
        infoObject["Device Name"] = std::string(info->name);
        infoObject["Host API Name"] = std::string(Pa_GetHostApiInfo(info->hostApi)->name);
        infoObject["Max Input Channels"] = info->maxInputChannels;
        infoObject["Max Output Channels"] = info->maxOutputChannels;
        infoObject["Default Sample Rate"] = info->defaultSampleRate;
        devicesArray.push_back(infoObject);
    }

    topObject["PortAudio Version"] = std::string(Pa_GetVersionText());

    Pa_Terminate();

    std::stringstream ss;
    ss << topObject;
    return ss.str();
}

pothos_static_block(registerAudioInfo)
{
    Pothos::PluginRegistry::addCall(
        "/devices/audio/info", &enumerateAudioDevices);
}
