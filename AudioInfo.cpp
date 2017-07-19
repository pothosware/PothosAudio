// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <portaudio.h>
#include <json.hpp>

using json = nlohmann::json;

static std::string enumerateAudioDevices(void)
{
    json topObject;
    Pa_Initialize();

    json devicesArray;
    for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); i++)
    {
        auto info = Pa_GetDeviceInfo(i);
        json infoObject;
        infoObject["Device Name"] = std::string(info->name);
        infoObject["Host API Name"] = std::string(Pa_GetHostApiInfo(info->hostApi)->name);
        infoObject["Max Input Channels"] = info->maxInputChannels;
        infoObject["Max Output Channels"] = info->maxOutputChannels;
        infoObject["Default Sample Rate"] = info->defaultSampleRate;
        devicesArray.push_back(infoObject);
    }

    topObject["PortAudio Device"] = devicesArray;
    topObject["PortAudio Version"] = Pa_GetVersionText();

    Pa_Terminate();

    return devicesArray.dump();
}

pothos_static_block(registerAudioInfo)
{
    Pothos::PluginRegistry::addCall(
        "/devices/audio/info", &enumerateAudioDevices);
}
