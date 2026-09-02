/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2025 Filipe Coelho <falktx@falktx.com>
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

/* Regression coverage for the AudioUnit buffer lists, which the AU wrapper itself cannot give us
 * anywhere but macOS. The interesting failure -- a bus list freed by a shape it no longer has, or
 * pointing at buffers somebody else owns -- is pure allocator behaviour and needs neither CoreAudio
 * nor a host, so the two types the class touches are declared here and it is tested directly.
 *
 * Worth running under ASan, where the mistakes this guards against are memory errors rather than
 * assertion failures: make -C tests ../build/tests/AudioUnitBufferList \
 *   BASE_OPTS="-O1 -g -fsanitize=address" LINK_OPTS="-fsanitize=address"
 */

#define DAF_TEST_NO_DGL
#include "tests.hpp"

// layout-compatible stand-ins for the CoreAudioTypes declarations
typedef unsigned int UInt32;

struct AudioBuffer {
    UInt32 mNumberChannels;
    UInt32 mDataByteSize;
    void* mData;
};

struct AudioBufferList {
    UInt32 mNumberBuffers;
    AudioBuffer mBuffers[1];
};

#include "daf/src/DafPluginAUBufferList.hpp"

// --------------------------------------------------------------------------------------------------------------------

/* What a connected AudioUnit or an AURenderCallback is allowed to do to the list we hand it: report
 * fewer buffers than we offered and point the ones it does report at memory of its own. */
static void pretendToBeAConnectedUnit(AudioBufferList* const list,
                                      const UInt32 numBuffers,
                                      float* const* const foreignBuffers)
{
    list->mNumberBuffers = numBuffers;

    for (UInt32 i = 0; i < numBuffers; ++i)
        list->mBuffers[i].mData = foreignBuffers[i];
}

// --------------------------------------------------------------------------------------------------------------------

int main()
{
    using DAF_NAMESPACE::AUBufferList;

    static constexpr const uint16_t kChannels = 2;
    static constexpr const uint32_t kFrames = 128;

    // a fresh list describes what was asked for, and owns every buffer in it
    {
        AUBufferList buffers;
        DAF_ASSERT_EQUAL(buffers.getList(), nullptr, "empty before allocating");
        DAF_ASSERT_EQUAL(buffers.allocate(kChannels, kFrames), true, "allocation succeeds");

        AudioBufferList* const list = buffers.getList();
        DAF_ASSERT_NOT_EQUAL(list, nullptr, "list exists after allocating");
        DAF_ASSERT_EQUAL(list->mNumberBuffers, kChannels, "list reports the requested channels");
        DAF_ASSERT_EQUAL(buffers.getNumBuffers(), kChannels, "allocation shape is the requested one");

        for (uint16_t i = 0; i < kChannels; ++i)
        {
            DAF_ASSERT_NOT_EQUAL(buffers.getChannelBuffer(i), nullptr, "channel buffer exists");
            DAF_ASSERT_EQUAL(list->mBuffers[i].mData, buffers.getChannelBuffer(i), "list points at our buffer");
            DAF_ASSERT_EQUAL(list->mBuffers[i].mDataByteSize, sizeof(float) * kFrames, "buffer size is the frame count");
        }

        DAF_ASSERT_EQUAL(buffers.getChannelBuffer(kChannels), nullptr, "no channel past the allocation");
    }

    // a callee that keeps the list but swaps in its own buffers must not cost us ours, and must not
    // get its own freed either: this is the teardown that corrupted the heap
    {
        float* foreign[kChannels];
        for (uint16_t i = 0; i < kChannels; ++i)
        {
            foreign[i] = new float[kFrames];
            foreign[i][0] = 42.0f;
        }

        {
            AUBufferList buffers;
            DAF_ASSERT_EQUAL(buffers.allocate(kChannels, kFrames), true, "allocation succeeds");

            // one narrower render, of the kind a bus reconfiguration produces
            AudioBufferList* const list = buffers.prepare(1, kFrames);
            DAF_ASSERT_NOT_EQUAL(list, nullptr, "narrower render is accepted");
            DAF_ASSERT_EQUAL(list->mNumberBuffers, 1u, "list reports the narrower width");
            DAF_ASSERT_EQUAL(list->mBuffers[kChannels - 1].mData, buffers.getChannelBuffer(kChannels - 1),
                             "the entries the render does not use stay ours");

            pretendToBeAConnectedUnit(list, kChannels, foreign);

            // the next render takes the list back, whatever the last one left in it
            AudioBufferList* const again = buffers.prepare(kChannels, kFrames);
            DAF_ASSERT_NOT_EQUAL(again, nullptr, "the list is reusable");

            for (uint16_t i = 0; i < kChannels; ++i)
                DAF_ASSERT_EQUAL(again->mBuffers[i].mData, buffers.getChannelBuffer(i),
                                 "foreign buffers are dropped before reuse");

            // and the last thing the unit sees before teardown is the foreign list again
            pretendToBeAConnectedUnit(again, kChannels, foreign);
        }

        for (uint16_t i = 0; i < kChannels; ++i)
        {
            DAF_ASSERT_SAFE_EQUAL(foreign[i][0], 42.0f, "the connected unit still owns its buffers");
            delete[] foreign[i];
        }
    }

    // repeated reconfiguration and teardown, the sequence the AU wrapper runs on every
    // initialize/uninitialize pair, with the width changing underneath it
    {
        AUBufferList buffers;

        for (uint16_t cycle = 0; cycle < 8; ++cycle)
        {
            const uint16_t channels = 1 + (cycle % kChannels);
            const uint32_t frames = kFrames * (1 + (cycle % 3));

            DAF_ASSERT_EQUAL(buffers.allocate(channels, frames), true, "reallocation succeeds");
            DAF_ASSERT_EQUAL(buffers.getNumBuffers(), channels, "shape follows the new configuration");

            AudioBufferList* const list = buffers.prepare(channels, frames);
            DAF_ASSERT_NOT_EQUAL(list, nullptr, "render at the new configuration");

            // scribble, so an undersized buffer is a memory error and not a silent pass
            for (uint16_t i = 0; i < channels; ++i)
                std::memset(list->mBuffers[i].mData, 0, list->mBuffers[i].mDataByteSize);

            buffers.deallocate();
            DAF_ASSERT_EQUAL(buffers.getList(), nullptr, "teardown empties the list");
            DAF_ASSERT_EQUAL(buffers.getNumBuffers(), 0, "teardown forgets the shape");

            // a second teardown is not a double free
            buffers.deallocate();
        }
    }

    // a render that does not fit the allocation is refused rather than overrunning it
    {
        AUBufferList buffers;
        DAF_ASSERT_EQUAL(buffers.allocate(kChannels, kFrames), true, "allocation succeeds");
        DAF_ASSERT_EQUAL(buffers.prepare(kChannels + 1, kFrames), nullptr, "too many channels is refused");
        DAF_ASSERT_EQUAL(buffers.prepare(kChannels, kFrames + 1), nullptr, "too many frames is refused");
        DAF_ASSERT_NOT_EQUAL(buffers.prepare(kChannels, kFrames), nullptr, "the allocated shape is accepted");
    }

    // an unallocated list refuses to render instead of dereferencing nothing
    {
        AUBufferList buffers;
        DAF_ASSERT_EQUAL(buffers.prepare(1, kFrames), nullptr, "no allocation, no render");
    }

    return 0;
}

// --------------------------------------------------------------------------------------------------------------------
