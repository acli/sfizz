// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "SIMDHelpers.h"
#include "Buffer.h"
#include <benchmark/benchmark.h>
#include <sndfile.hh>
#include "ghc/filesystem.hpp"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "AudioBuffer.h"
#include "absl/memory/memory.h"
#ifndef NDEBUG
#include <iostream>
#endif

class FileFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& /* state */) {
        rootPath = getPath() / "sample1.wav";
        if (!ghc::filesystem::exists(rootPath)) {
        #ifndef NDEBUG
            std::cerr << "Can't find path" << '\n';
        #endif
            std::terminate();
        }

        sndfile = SndfileHandle(rootPath.c_str());
        numFrames = static_cast<size_t>(sndfile.frames());
        output = absl::make_unique<sfz::AudioBuffer<float>>(sndfile.channels(), sndfile.frames());
    }

    void TearDown(const ::benchmark::State& /* state */) {
    }

    ghc::filesystem::path getPath()
    {
        #ifdef __linux__
        char buf[PATH_MAX + 1];
        if (readlink("/proc/self/exe", buf, sizeof(buf) - 1) == -1)
            return {};
        std::string str { buf };
        return str.substr(0, str.rfind('/'));
        #elif _WIN32
        return ghc::filesystem::current_path();
        #endif
    }

    std::unique_ptr<sfz::AudioBuffer<float>> output;
    SndfileHandle sndfile;
    ghc::filesystem::path rootPath;
    size_t numFrames;
};


BENCHMARK_DEFINE_F(FileFixture, JustRead)(benchmark::State& state) {
    for (auto _ : state)
    {
        sfz::Buffer<float> buffer { numFrames * sndfile.channels() };
        sndfile.readf(buffer.data(), numFrames);
        sfz::readInterleaved<float>(buffer, output->getSpan(0), output->getSpan(1));
    }
}

BENCHMARK_DEFINE_F(FileFixture, AllocInside)(benchmark::State& state) {
    for (auto _ : state)
    {
        auto chunkSize = static_cast<size_t>(state.range(0));
        size_t framesRead = 0;
        sndfile.seek(0, SEEK_SET);
        while(framesRead < numFrames)
        {
            sfz::Buffer<float> buffer { chunkSize * sndfile.channels() };
            auto read = sndfile.readf(buffer.data(), chunkSize);
            sfz::readInterleaved<float>(
                absl::MakeSpan(buffer).first(read),
                output->getSpan(0).subspan(framesRead),
                output->getSpan(1).subspan(framesRead)
            );
            framesRead += read;
        }
    }
}

BENCHMARK_DEFINE_F(FileFixture, AllocOutside)(benchmark::State& state) {
    auto chunkSize = static_cast<size_t>(state.range(0));
    sfz::Buffer<float> buffer { chunkSize * sndfile.channels() };
    for (auto _ : state)
    {
        size_t framesRead = 0;
        sndfile.seek(0, SEEK_SET);
        while(framesRead < numFrames)
        {
            auto read = sndfile.readf(buffer.data(), chunkSize);
            sfz::readInterleaved<float>(
                absl::MakeSpan(buffer).first(read),
                output->getSpan(0).subspan(framesRead),
                output->getSpan(1).subspan(framesRead)
            );
            framesRead += read;
        }
    }
}


BENCHMARK_DEFINE_F(FileFixture, DrWavChunked)(benchmark::State& state) {
    auto chunkSize = static_cast<size_t>(state.range(0));
    drwav wav;
    if (!drwav_init_file(&wav, rootPath.c_str(), nullptr)) {
        #ifndef NDEBUG
            std::cerr << "Can't find path" << '\n';
        #endif
        std::terminate();
    }
    sfz::Buffer<float> buffer { chunkSize * wav.channels };

    for (auto _ : state)
    {
        size_t framesRead = 0;
        drwav_seek_to_first_pcm_frame(&wav);
        while(framesRead < numFrames)
        {
            auto read = drwav_read_pcm_frames_f32(&wav, chunkSize, buffer.data());
            sfz::readInterleaved<float>(
                absl::MakeSpan(buffer).first(read),
                output->getSpan(0).subspan(framesRead),
                output->getSpan(1).subspan(framesRead)
            );
            framesRead += read;
        }
    }
}

BENCHMARK_REGISTER_F(FileFixture, JustRead);
BENCHMARK_REGISTER_F(FileFixture, AllocInside)->RangeMultiplier(4)->Range((1 << 8), (1 << 16));
BENCHMARK_REGISTER_F(FileFixture, AllocOutside)->RangeMultiplier(4)->Range((1 << 8), (1 << 16));
BENCHMARK_REGISTER_F(FileFixture, DrWavChunked)->RangeMultiplier(4)->Range((1 << 8), (1 << 16));
BENCHMARK_MAIN();
