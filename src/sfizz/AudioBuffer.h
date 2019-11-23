// Copyright (c) 2019, Paul Ferrand
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once
#include "Buffer.h"
#include "Config.h"
#include "Debug.h"
#include "LeakDetector.h"
#include "absl/types/span.h"
#include <memory>
#include <array>

namespace sfz 
{
/**
 * @brief A class to handle a collection of buffers, where each buffer has the same size.
 * 
 * Unlike AudioSpan, this class *owns* its underlying buffers and they are freed when the buffer
 * is destroyed. 
 * 
 * @tparam Type the underlying type of the buffers
 * @tparam MaxChannels the maximum number of channels in the buffer
 * @tparam Alignment the alignment for the buffers
 */
template <class Type, unsigned int MaxChannels = sfz::config::numChannels, unsigned int Alignment = SIMDConfig::defaultAlignment>
class AudioBuffer {
public:
    using value_type = std::remove_cv_t<Type>;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using size_type = size_t;

    /**
     * @brief Construct a new Audio Buffer object
     * 
     */
    AudioBuffer()
    {
    }

    /**
     * @brief Construct a new Audio Buffer object with a specified number of
     * channels and frames.
     * 
     * @param numChannels 
     * @param numFrames 
     */
    AudioBuffer(int numChannels, int numFrames)
        : numChannels(numChannels)
        , numFrames(numFrames)
    {
        for (auto i = 0; i < numChannels; ++i)
            buffers[i] = std::make_unique<buffer_type>(numFrames);
    }

    /**
     * @brief Resizes all the underlying buffers to a new size.
     * 
     * @param newSize 
     * @return true if the resize worked
     * @return false otherwise
     */
    bool resize(size_type newSize)
    {
        bool returnedOK = true;
        for (auto i = 0; i < numChannels; ++i)
            returnedOK &= buffers[i]->resize(newSize);
        return returnedOK;
    }

    /**
     * @brief Return an iterator to a specific channel with a non-const type.
     * 
     * @param channelIndex 
     * @return iterator 
     */
    iterator channelWriter(int channelIndex)
    {
        ASSERT(channelIndex < numChannels)
        if (channelIndex < numChannels)
            return buffers[channelIndex]->data();

        return {};
    }

    /**
     * @brief Returns a sentinel for the channelWriter(channelIndex) iterator
     * 
     * @param channelIndex 
     * @return iterator 
     */
    iterator channelWriterEnd(int channelIndex)
    {
        ASSERT(channelIndex < numChannels)
        if (channelIndex < numChannels)
            return buffers[channelIndex]->end();

        return {};
    }

    /**
     * @brief Returns a const iterator for a specific channel
     * 
     * @param channelIndex 
     * @return const_iterator 
     */
    const_iterator channelReader(int channelIndex) const
    {
        ASSERT(channelIndex < numChannels)
        if (channelIndex < numChannels)
            return buffers[channelIndex]->data();

        return {};
    }

    /**
     * @brief Returns a sentinel for the channelReader(channelIndex) iterator
     * 
     * @param channelIndex 
     * @return const_iterator 
     */
    const_iterator channelReaderEnd(int channelIndex) const
    {
        ASSERT(channelIndex < numChannels)
        if (channelIndex < numChannels)
            return buffers[channelIndex]->end();

        return {};
    }

    /**
     * @brief Get a Span for a specific channel
     * 
     * @param channelIndex 
     * @return absl::Span<value_type> 
     */
    absl::Span<value_type> getSpan(int channelIndex) const
    {
        ASSERT(channelIndex < numChannels)
        if (channelIndex < numChannels)
            return { buffers[channelIndex]->data(), buffers[channelIndex]->size() };

        return {};
    }

    /**
     * @brief Get a const Span object for a specific channel
     * 
     * @param channelIndex 
     * @return absl::Span<const value_type> 
     */
    absl::Span<const value_type> getConstSpan(int channelIndex) const
    {
        return getSpan(channelIndex);
    }

    /**
     * @brief Add a channel to the buffer with the current number of frames.
     * 
     */
    void addChannel()
    {
        if (numChannels < MaxChannels)
            buffers[numChannels++] = std::make_unique<buffer_type>(numFrames);
    }

    /**
     * @brief Get the number of elements in each buffer
     * 
     * @return size_type 
     */
    size_type getNumFrames() const
    {
        return numFrames;
    }

    /**
     * @brief Get the number of channels
     * 
     * @return int 
     */
    int getNumChannels() const
    {
        return numChannels;
    }

    /**
     * @brief Check if the buffers contains no elements
     * 
     * @return true
     * @return false 
     */
    bool empty() const
    {
        return numFrames == 0;
    }

    /**
     * @brief Get a reference to a given element in a given buffer.
     * 
     * In release builds this is not checked and may touch bad memory.
     * 
     * @param channelIndex 
     * @param frameIndex 
     * @return Type& 
     */
    Type& getSample(int channelIndex, size_type frameIndex)
    {
        // Uhoh
        ASSERT(buffers[channelIndex] != nullptr);
        ASSERT(frameIndex < numFrames);

        return *(buffers[channelIndex]->data() + frameIndex);
    }

    /**
     * @brief Alias for getSample(...)
     * 
     * @param channelIndex 
     * @param frameIndex 
     * @return Type& 
     */
    Type& operator()(int channelIndex, size_type frameIndex)
    {
        return getSample(channelIndex, frameIndex);
    }

private:
    using buffer_type = Buffer<Type, Alignment>;
    using buffer_ptr = std::unique_ptr<buffer_type>;
    std::array<buffer_ptr, MaxChannels> buffers;
    int numChannels { 0 };
    size_type numFrames { 0 };
};
}