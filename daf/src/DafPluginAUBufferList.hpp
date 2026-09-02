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

#ifndef DAF_PLUGIN_AU_BUFFER_LIST_HPP_INCLUDED
#define DAF_PLUGIN_AU_BUFFER_LIST_HPP_INCLUDED

#include "../DafUtils.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

/* AudioBuffer and AudioBufferList must already be declared; on macOS that is
 * <CoreAudioTypes/CoreAudioTypes.h>, reached through <AudioUnit/AudioUnit.h>.
 * The unit test declares layout-compatible stand-ins so this file can be built
 * and exercised on any platform. */

START_NAMESPACE_DAF

// --------------------------------------------------------------------------------------------------------------------

/**
   An AudioBufferList plus the channel buffers behind it, owned as one unit.

   The point of the class is that an AudioBufferList handed out to somebody else does not describe
   what we allocated any more. Both AudioUnitRender and an AURenderCallback are allowed to hand back
   a list whose mNumberBuffers has shrunk and whose mBuffers[].mData point at buffers the callee
   owns, and they routinely do: pointing the list at an existing buffer instead of copying into ours
   is the whole point of the AU pull model. Freeing such a list by walking mNumberBuffers and
   deleting mData corrupts the heap, and a bus that is reconfigured between renders makes the
   mismatch permanent rather than transient.

   So the allocation shape and the pointers we own are kept here and are never read back out of the
   list. deallocate() frees exactly what allocate() produced, whatever state the list was left in,
   and prepare() puts our own pointers and sizes back before every use.
 */
class AUBufferList
{
public:
    AUBufferList() noexcept
        : fList(nullptr),
          fData(nullptr),
          fNumBuffers(0),
          fBufferSize(0) {}

    ~AUBufferList() noexcept
    {
        deallocate();
    }

    /**
       Allocate a list of @a numBuffers mono buffers of @a bufferSize frames each.
       Any previous allocation is released first. Returns false on allocation failure, leaving the
       object empty rather than half built.
     */
    bool allocate(const uint16_t numBuffers, const uint32_t bufferSize) noexcept
    {
        deallocate();

        AudioBufferList* const list = static_cast<AudioBufferList*>(
            std::malloc(offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer) * numBuffers));

        if (list == nullptr)
            return false;

        float** const data = numBuffers != 0 ? new (std::nothrow) float*[numBuffers] : nullptr;

        if (numBuffers != 0 && data == nullptr)
        {
            std::free(list);
            return false;
        }

        for (uint16_t i = 0; i < numBuffers; ++i)
            data[i] = nullptr;

        list->mNumberBuffers = numBuffers;

        for (uint16_t i = 0; i < numBuffers; ++i)
        {
            data[i] = new (std::nothrow) float[bufferSize];

            if (data[i] == nullptr)
            {
                for (uint16_t j = 0; j < i; ++j)
                    delete[] data[j];
                delete[] data;
                std::free(list);
                return false;
            }

            std::memset(data[i], 0, sizeof(float) * bufferSize);

            list->mBuffers[i].mNumberChannels = 1;
            list->mBuffers[i].mData = data[i];
            list->mBuffers[i].mDataByteSize = sizeof(float) * bufferSize;
        }

        fList = list;
        fData = data;
        fNumBuffers = numBuffers;
        fBufferSize = bufferSize;
        return true;
    }

    /**
       Release the allocation, using the shape it was created with rather than anything the list
       currently says. Safe to call on an empty object and safe to call twice.
     */
    void deallocate() noexcept
    {
        if (fData != nullptr)
        {
            for (uint16_t i = 0; i < fNumBuffers; ++i)
                delete[] fData[i];

            delete[] fData;
            fData = nullptr;
        }

        std::free(fList);
        fList = nullptr;
        fNumBuffers = 0;
        fBufferSize = 0;
    }

    /**
       Restore the list to describe @a channels of our own buffers, @a frames long, and return it
       ready to be handed to a render callback.

       Every allocated entry is pointed back at its own buffer, not just the @a channels the list
       will report: a bus that is narrower than it once was still has to leave its unused entries
       safe to memset, and those are exactly the ones a previous, wider render could have left
       pointing at somebody else's memory.

       Returns null if the request does not fit the allocation, so a caller that has somehow outrun
       its own buffers fails the render instead of overrunning them.
     */
    AudioBufferList* prepare(const uint16_t channels, const uint32_t frames) noexcept
    {
        if (fList == nullptr || channels > fNumBuffers || frames > fBufferSize)
            return nullptr;

        for (uint16_t i = 0; i < fNumBuffers; ++i)
        {
            fList->mBuffers[i].mNumberChannels = 1;
            fList->mBuffers[i].mData = fData[i];
            fList->mBuffers[i].mDataByteSize = sizeof(float) * frames;
        }

        fList->mNumberBuffers = channels;
        return fList;
    }

    /**
       The list as it currently stands, without restoring anything. Null until allocate() succeeds.
     */
    AudioBufferList* getList() const noexcept
    {
        return fList;
    }

    /**
       Number of buffers the allocation holds, which is not necessarily what the list reports.
     */
    uint16_t getNumBuffers() const noexcept
    {
        return fNumBuffers;
    }

    /**
       One of our own channel buffers, regardless of what the list points at. Null when out of range.
     */
    float* getChannelBuffer(const uint16_t channel) const noexcept
    {
        return channel < fNumBuffers ? fData[channel] : nullptr;
    }

private:
    AudioBufferList* fList;
    float** fData;
    uint16_t fNumBuffers;
    uint32_t fBufferSize;

    DAF_DECLARE_NON_COPYABLE(AUBufferList)
};

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DAF

#endif // DAF_PLUGIN_AU_BUFFER_LIST_HPP_INCLUDED
