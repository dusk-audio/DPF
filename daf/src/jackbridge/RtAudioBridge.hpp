/*
 * RtAudio Bridge for DAF
 * Copyright (C) 2021-2025 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef RTAUDIO_BRIDGE_HPP_INCLUDED
#define RTAUDIO_BRIDGE_HPP_INCLUDED

#include "NativeBridge.hpp"

#if (DAF_PLUGIN_NUM_INPUTS + DAF_PLUGIN_NUM_OUTPUTS) == 0
# error RtAudio without audio does not make sense
#endif

#if defined(DAF_OS_MAC)
# define __MACOSX_CORE__
# define RTAUDIO_API_TYPE MACOSX_CORE
# define RTMIDI_API_TYPE MACOSX_CORE
#elif defined(DAF_OS_WINDOWS)
// NOTE the vendored RtAudio WASAPI and RtMidi WinMM backends build under MSVC as well as MinGW.
// The one thing they cannot handle there is HAVE_GETTIMEOFDAY, which pulls in <sys/time.h> and
// gettimeofday(); the build files therefore only define it for non-MSVC compilers.
# define __WINDOWS_WASAPI__
# define __WINDOWS_MM__
# define RTAUDIO_API_TYPE WINDOWS_WASAPI
# define RTMIDI_API_TYPE WINDOWS_MM
#else
# if defined(HAVE_PULSEAUDIO) && !defined(DAF_JACK_STANDALONE_SKIP_PULSEAUDIO_FALLBACK)
#  define __LINUX_PULSE__
#  define RTAUDIO_API_TYPE LINUX_PULSE
# elif defined(HAVE_ALSA)
#  define RTAUDIO_API_TYPE LINUX_ALSA
# endif
# ifdef HAVE_ALSA
#  define __LINUX_ALSA__
#  define RTMIDI_API_TYPE LINUX_ALSA
# endif
#endif

// NOTE __PRETTY_FUNCTION__ is a GCC/Clang builtin, MSVC spells the same thing __FUNCSIG__.
#ifdef _MSC_VER
# define RTAUDIO_BRIDGE_FUNC_NAME __FUNCSIG__
#else
# define RTAUDIO_BRIDGE_FUNC_NAME __PRETTY_FUNCTION__
#endif

#ifdef RTAUDIO_API_TYPE
# include "rtaudio/RtAudio.h"
# include "rtmidi/RtMidi.h"
# include "../../extra/ScopedPointer.hpp"
# include "../../extra/String.hpp"
# include "../../extra/ScopedDenormalDisable.hpp"

using DAF_NAMESPACE::ScopedDenormalDisable;
using DAF_NAMESPACE::ScopedPointer;
using DAF_NAMESPACE::String;

struct RtAudioBridge : NativeBridge {
    // pointer to RtAudio instance
    ScopedPointer<RtAudio> handle;
    bool captureEnabled = false;
   #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_INPUT
    std::vector<RtMidiIn> midiIns;
   #endif
   #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_OUTPUT
    std::vector<RtMidiOut> midiOuts;
   #endif

    // caching
    String name;
    uint nextBufferSize = 512;

    RtAudioBridge()
    {
       #if defined(RTMIDI_API_TYPE) && (DAF_PLUGIN_WANT_MIDI_INPUT || DAF_PLUGIN_WANT_MIDI_OUTPUT)
        midiAvailable = true;
       #endif
    }

    const char* getVersion() const noexcept
    {
        return RTAUDIO_VERSION;
    }

    bool open(const char* const clientName) override
    {
        name = clientName;
        return _open(false);
    }

    bool close() override
    {
        DAF_SAFE_ASSERT_RETURN(handle != nullptr, false);

        if (handle->isStreamRunning())
        {
            try {
                handle->abortStream();
            } DAF_SAFE_EXCEPTION("handle->abortStream()");
        }

       #if DAF_PLUGIN_NUM_INPUTS > 0
        freeBuffers();
       #endif
        handle = nullptr;
        return true;
    }

    bool activate() override
    {
        DAF_SAFE_ASSERT_RETURN(handle != nullptr, false);

        try {
            handle->startStream();
        } DAF_SAFE_EXCEPTION_RETURN("handle->startStream()", false);

        return true;
    }

    bool deactivate() override
    {
        DAF_SAFE_ASSERT_RETURN(handle != nullptr, false);

        try {
            handle->stopStream();
        } DAF_SAFE_EXCEPTION_RETURN("handle->stopStream()", false);

        return true;
    }

    bool isAudioInputEnabled() const override
    {
       #if DAF_PLUGIN_NUM_INPUTS > 0
        return captureEnabled;
       #else
        return false;
       #endif
    }

    bool requestAudioInput() override
    {
       #if DAF_PLUGIN_NUM_INPUTS > 0
        // stop audio first
        deactivate();
        close();

        // try to open with capture enabled
        const bool ok = _open(true);

        if (ok)
            captureEnabled = true;
        else
            _open(false);

        activate();
        return ok;
       #else
        return false;
       #endif
    }

    bool isMIDIEnabled() const override
    {
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_INPUT
        if (!midiIns.empty())
            return true;
       #endif
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_OUTPUT
        if (!midiOuts.empty())
            return true;
       #endif
        return false;
    }

    bool requestMIDI() override
    {
        d_stdout("%s %d", RTAUDIO_BRIDGE_FUNC_NAME, __LINE__);
        // clear ports in use first
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_INPUT
        if (!midiIns.empty())
        {
            try {
                midiIns.clear();
            } catch (const RtMidiError& err) {
                d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
                return false;
            } DAF_SAFE_EXCEPTION_RETURN("midiIns.clear()", false);
        }
       #endif
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_OUTPUT
        if (!midiOuts.size())
        {
            try {
                midiOuts.clear();
            } catch (const RtMidiError& err) {
                d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
                return false;
            } DAF_SAFE_EXCEPTION_RETURN("midiOuts.clear()", false);
        }
       #endif

        // query port count
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_INPUT
        uint midiInCount;
        try {
            RtMidiIn midiIn(RtMidi::RTMIDI_API_TYPE, name.buffer());
            midiInCount = midiIn.getPortCount();
        } catch (const RtMidiError& err) {
            d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
            return false;
        } DAF_SAFE_EXCEPTION_RETURN("midiIn.getPortCount()", false);
       #endif
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_OUTPUT
        uint midiOutCount;
        try {
            RtMidiOut midiOut(RtMidi::RTMIDI_API_TYPE, name.buffer());
            midiOutCount = midiOut.getPortCount();
        } catch (const RtMidiError& err) {
            d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
            return false;
        } DAF_SAFE_EXCEPTION_RETURN("midiOut.getPortCount()", false);
       #endif

        // open all possible ports
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_INPUT
        for (uint i=0; i<midiInCount; ++i)
        {
            try {
                RtMidiIn midiIn(RtMidi::RTMIDI_API_TYPE, name.buffer());
                midiIn.setCallback(RtMidiCallback, this);
                midiIn.openPort(i);
                midiIns.push_back(std::move(midiIn));
            } catch (const RtMidiError& err) {
                d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
            } DAF_SAFE_EXCEPTION("midiIn.openPort()");
        }
       #endif
       #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_OUTPUT
        for (uint i=0; i<midiOutCount; ++i)
        {
            try {
                RtMidiOut midiOut(RtMidi::RTMIDI_API_TYPE, name.buffer());
                midiOut.openPort(i);
                midiOuts.push_back(std::move(midiOut));
            } catch (const RtMidiError& err) {
                d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
            } DAF_SAFE_EXCEPTION("midiOut.openPort()");
        }
       #endif

        return true;
    }

    bool supportsBufferSizeChanges() const override
    {
        return true;
    }

    bool requestBufferSizeChange(const uint32_t newBufferSize) override
    {
        // stop audio first
        deactivate();
        close();

        // try to open with new buffer size
        nextBufferSize = newBufferSize;

        const bool ok = _open(captureEnabled);

        if (!ok)
        {
            // revert to old buffer size if new one failed
            nextBufferSize = bufferSize;
            _open(captureEnabled);
        }

        if (bufferSizeCallback != nullptr)
            bufferSizeCallback(bufferSize, jackBufferSizeArg);

        activate();
        return ok;
    }

    bool _open(const bool withInput, RtAudio* tryingAgain = nullptr)
    {
        ScopedPointer<RtAudio> rtAudio;

        if (tryingAgain == nullptr)
        {
            try {
                rtAudio = new RtAudio(RtAudio::RTAUDIO_API_TYPE);
            } DAF_SAFE_EXCEPTION_RETURN("new RtAudio()", false);
        }
        else
        {
            rtAudio = tryingAgain;
        }

        uint rtAudioBufferFrames = nextBufferSize;

       #if DAF_PLUGIN_NUM_INPUTS > 0
        RtAudio::StreamParameters inParams;
       #endif
        RtAudio::StreamParameters* inParamsPtr = nullptr;

       #if DAF_PLUGIN_NUM_INPUTS > 0
        if (withInput)
        {
            inParams.deviceId = rtAudio->getDefaultInputDevice();
            inParams.nChannels = DAF_PLUGIN_NUM_INPUTS_2;
            inParamsPtr = &inParams;
        }
       #endif

       #if DAF_PLUGIN_NUM_OUTPUTS > 0
        RtAudio::StreamParameters outParams;
        outParams.deviceId = tryingAgain != nullptr ? 1 : rtAudio->getDefaultOutputDevice();
        outParams.nChannels = DAF_PLUGIN_NUM_OUTPUTS_2;
        RtAudio::StreamParameters* const outParamsPtr = &outParams;
       #else
        RtAudio::StreamParameters* const outParamsPtr = nullptr;
       #endif

        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_NONINTERLEAVED | RTAUDIO_ALSA_USE_DEFAULT;
       #ifndef DAF_OS_MAC
       /* RtAudio in macOS uses a different than usual way to handle audio block size,
        * where RTAUDIO_MINIMIZE_LATENCY makes CoreAudio use very low latencies (around 15 samples).
        * That has serious performance drawbacks, so we skip that here.
        */
        opts.flags |= RTAUDIO_MINIMIZE_LATENCY;
       #endif
        opts.numberOfBuffers = 2;
        opts.streamName = name.buffer();

        try {
            rtAudio->openStream(outParamsPtr, inParamsPtr, RTAUDIO_FLOAT32, 48000, &rtAudioBufferFrames,
                                RtAudioCallback, this, &opts, nullptr);
        } catch (const RtAudioError& err) {
           #if DAF_PLUGIN_NUM_OUTPUTS > 0
            if (outParams.deviceId == 0 && rtAudio->getDeviceCount() > 1)
                return _open(withInput, rtAudio.release());
           #endif
            d_safe_exception(err.getMessage().c_str(), __FILE__, __LINE__);
            return false;
        } DAF_SAFE_EXCEPTION_RETURN("rtAudio->openStream()", false);

        handle = rtAudio;
        bufferSize = rtAudioBufferFrames;
        sampleRate = handle->getStreamSampleRate();
        allocBuffers(!withInput, true);
        return true;
    }

    static int RtAudioCallback(void* const outputBuffer,
                              #if DAF_PLUGIN_NUM_INPUTS > 0
                               void* const inputBuffer,
                              #else
                               void*,
                              #endif
                               const uint numFrames,
                               const double /* streamTime */,
                               const RtAudioStreamStatus /* status */,
                               void* const userData)
    {
        RtAudioBridge* const self = static_cast<RtAudioBridge*>(userData);

        if (self->jackProcessCallback == nullptr)
        {
            if (outputBuffer != nullptr)
                std::memset((float*)outputBuffer, 0, sizeof(float)*numFrames*DAF_PLUGIN_NUM_OUTPUTS_2);
            return 0;
        }

       #if DAF_PLUGIN_NUM_INPUTS > 0
        if (float* const insPtr = static_cast<float*>(inputBuffer))
        {
            for (uint i=0; i<DAF_PLUGIN_NUM_INPUTS_2; ++i)
                self->audioBuffers[i] = insPtr + (i * numFrames);
        }
       #endif

       #if DAF_PLUGIN_NUM_OUTPUTS > 0
        if (float* const outsPtr = static_cast<float*>(outputBuffer))
        {
            for (uint i=0; i<DAF_PLUGIN_NUM_OUTPUTS_2; ++i)
                self->audioBuffers[DAF_PLUGIN_NUM_INPUTS + i] = outsPtr + (i * numFrames);
        }
       #endif

        const ScopedDenormalDisable sdd;
        self->jackProcessCallback(numFrames, self->jackProcessArg);

       #if DAF_PLUGIN_WANT_MIDI_OUTPUT
        if (self->midiOutBuffer.isDataAvailableForReading())
        {
            static_assert(kMaxMIDIInputMessageSize + 1u == 4, "change code if bumping this value");
            uint8_t data[4] = {};

            while (self->midiOutBuffer.isDataAvailableForReading() &&
                    self->midiOutBuffer.readCustomData(data, ARRAY_SIZE(data)))
            {
                // offset not used in RtMidiOut
                self->midiOutBuffer.readUInt();

                for (std::vector<RtMidiOut>::iterator it = self->midiOuts.begin(), end = self->midiOuts.end(); it != end; ++it)
                {
                    static_cast<RtMidiOut&>(*it).sendMessage(data + 1, data[0]);
                }
            }

            self->midiOutBuffer.flush();
        }
       #endif

        return 0;
    }

   #if defined(RTMIDI_API_TYPE) && DAF_PLUGIN_WANT_MIDI_INPUT
    static void RtMidiCallback(double /*timestamp*/, std::vector<uchar>* const message, void* const userData)
    {
        const size_t len = message->size();
        DAF_SAFE_ASSERT_RETURN(len > 0 && len <= kMaxMIDIInputMessageSize,);

        RtAudioBridge* const self = static_cast<RtAudioBridge*>(userData);

        const RecursiveMutexLocker rml(self->midiInLock);

        self->midiInBufferPending.writeByte(static_cast<uint8_t>(len));
        // TODO timestamp
        // self->midiInBufferPending.writeDouble(timestamp);
        self->midiInBufferPending.writeCustomData(message->data(), len);
        for (uint8_t i = len; i < kMaxMIDIInputMessageSize; ++i)
            self->midiInBufferPending.writeByte(0);
        self->midiInBufferPending.commitWrite("RtMidiCallback");
    }
   #endif
};

#endif // RTAUDIO_API_TYPE
#endif // RTAUDIO_BRIDGE_HPP_INCLUDED
