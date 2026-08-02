// Tests for the wav reader.
//
// This parses attacker-shaped data in the sense that matters here: chunk sizes
// come out of the file and are used to index into it. A truncated or hand-edited
// wav is an entirely plausible accident -- an interrupted asset generation, a
// bad merge on a binary file -- and must produce a clean failure rather than a
// read past the end of the buffer.
//
// It is also the only part of the audio layer that can be tested at all without
// a sound device, which is every CI runner.

#include "Audio/WavLoader.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

// Builds a minimal valid 16-bit PCM wav in memory.
std::vector<std::uint8_t> makeWav(int channels, int sampleRate, int frameCount,
                                  std::uint16_t format = 1, std::uint16_t bits = 16) {
    const int bytesPerFrame = channels * (bits / 8);
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(frameCount * bytesPerFrame);

    std::vector<std::uint8_t> out;
    auto push32 = [&out](std::uint32_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    };
    auto push16 = [&out](std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    };
    auto pushTag = [&out](const char* tag) {
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<std::uint8_t>(tag[i]));
        }
    };

    pushTag("RIFF");
    push32(36 + dataBytes);
    pushTag("WAVE");

    pushTag("fmt ");
    push32(16);
    push16(format);
    push16(static_cast<std::uint16_t>(channels));
    push32(static_cast<std::uint32_t>(sampleRate));
    push32(static_cast<std::uint32_t>(sampleRate * bytesPerFrame));   // byte rate
    push16(static_cast<std::uint16_t>(bytesPerFrame));                // block align
    push16(bits);

    pushTag("data");
    push32(dataBytes);
    for (std::uint32_t i = 0; i < dataBytes; ++i) {
        out.push_back(static_cast<std::uint8_t>(i & 0xFF));
    }
    return out;
}

bool parse(const std::vector<std::uint8_t>& bytes, hu::WavData& out, std::string* err = nullptr) {
    return hu::parseWav(bytes.data(), bytes.size(), out, err);
}

} // namespace

TEST_CASE("A well-formed mono wav parses", "[audio][wav]") {
    hu::WavData wav;
    REQUIRE(parse(makeWav(1, 44100, 100), wav));

    CHECK(wav.channels == 1);
    CHECK(wav.sampleRate == 44100);
    CHECK(wav.bitsPerSample == 16);
    CHECK(wav.frameCount() == 100);
    CHECK(wav.samples.size() == 200);
    CHECK(wav.valid());
}

TEST_CASE("A well-formed stereo wav parses", "[audio][wav]") {
    hu::WavData wav;
    REQUIRE(parse(makeWav(2, 22050, 64), wav));

    CHECK(wav.channels == 2);
    CHECK(wav.sampleRate == 22050);
    CHECK(wav.frameCount() == 64);      // frames, not samples
    CHECK(wav.samples.size() == 256);   // 64 frames * 2 channels * 2 bytes
}

TEST_CASE("Duration is derived from the frame count", "[audio][wav]") {
    hu::WavData wav;
    REQUIRE(parse(makeWav(2, 1000, 500), wav));
    CHECK(wav.durationSeconds() > 0.49f);
    CHECK(wav.durationSeconds() < 0.51f);
}

TEST_CASE("Chunks between fmt and data are skipped", "[audio][wav]") {
    // Writers are entitled to insert LIST or fact chunks, and some do. Assuming
    // data immediately follows fmt would reject those files.
    std::vector<std::uint8_t> bytes = makeWav(1, 44100, 8);

    const std::size_t insertAt = 36;   // just past the fmt chunk
    const std::vector<std::uint8_t> listChunk = {
        'L', 'I', 'S', 'T', 0x04, 0x00, 0x00, 0x00, 'I', 'N', 'F', 'O'
    };
    bytes.insert(bytes.begin() + insertAt, listChunk.begin(), listChunk.end());

    hu::WavData wav;
    REQUIRE(parse(bytes, wav));
    CHECK(wav.frameCount() == 8);
}

TEST_CASE("An odd-sized chunk's pad byte is honoured", "[audio][wav]") {
    // RIFF chunks are word-aligned: an odd size is followed by a pad byte that
    // is not counted in the size. Missing it desynchronises every later chunk.
    std::vector<std::uint8_t> bytes = makeWav(1, 44100, 8);
    const std::vector<std::uint8_t> oddChunk = {
        'j', 'u', 'n', 'k', 0x03, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0x00
    };
    bytes.insert(bytes.begin() + 36, oddChunk.begin(), oddChunk.end());

    hu::WavData wav;
    REQUIRE(parse(bytes, wav));
    CHECK(wav.frameCount() == 8);
}

TEST_CASE("Malformed input is rejected rather than read past", "[audio][wav]") {
    hu::WavData wav;
    std::string error;

    SECTION("empty buffer") {
        CHECK_FALSE(hu::parseWav(nullptr, 0, wav, &error));
        CHECK_FALSE(error.empty());
    }

    SECTION("not a RIFF file") {
        std::vector<std::uint8_t> bytes(64, 0x00);
        CHECK_FALSE(parse(bytes, wav, &error));
        CHECK(error.find("RIFF") != std::string::npos);
    }

    SECTION("truncated mid-header") {
        std::vector<std::uint8_t> bytes = makeWav(1, 44100, 100);
        bytes.resize(20);
        CHECK_FALSE(parse(bytes, wav, &error));
    }

    SECTION("no data chunk") {
        std::vector<std::uint8_t> bytes = makeWav(1, 44100, 100);
        bytes.resize(36);   // header + fmt only
        CHECK_FALSE(parse(bytes, wav, &error));
        CHECK(error.find("data") != std::string::npos);
    }
}

TEST_CASE("Unsupported formats are rejected with a reason", "[audio][wav]") {
    hu::WavData wav;
    std::string error;

    SECTION("compressed") {
        // Format 3 is IEEE float; anything but 1 is not raw PCM.
        CHECK_FALSE(parse(makeWav(1, 44100, 10, /*format=*/3), wav, &error));
        CHECK(error.find("PCM") != std::string::npos);
    }

    SECTION("8-bit") {
        CHECK_FALSE(parse(makeWav(1, 44100, 10, 1, /*bits=*/8), wav, &error));
        CHECK(error.find("16-bit") != std::string::npos);
    }

    SECTION("too many channels") {
        CHECK_FALSE(parse(makeWav(6, 44100, 10), wav, &error));
        CHECK(error.find("mono") != std::string::npos);
    }
}

TEST_CASE("A data chunk longer than the file is clamped, not trusted", "[audio][wav]") {
    // The length field is data, and believing it over the actual buffer size is
    // how a malformed file turns into an out-of-bounds read.
    std::vector<std::uint8_t> bytes = makeWav(1, 44100, 50);
    // Overwrite the data chunk size (at offset 40) with something enormous.
    bytes[40] = 0xFF;
    bytes[41] = 0xFF;
    bytes[42] = 0x00;
    bytes[43] = 0x00;

    hu::WavData wav;
    REQUIRE(parse(bytes, wav));
    CHECK(wav.frameCount() == 50);   // what was actually there
}

TEST_CASE("A trailing partial frame is dropped", "[audio][wav]") {
    // Stereo 16-bit means four bytes per frame; a file ending mid-frame would
    // otherwise leave consumers with a size that does not divide evenly.
    std::vector<std::uint8_t> bytes = makeWav(2, 44100, 10);
    bytes.pop_back();
    bytes.pop_back();   // remove half a frame

    hu::WavData wav;
    REQUIRE(parse(bytes, wav));
    CHECK(wav.samples.size() % 4 == 0);
    CHECK(wav.frameCount() == 9);
}

TEST_CASE("Every generated asset loads", "[audio][wav]") {
    // Guards the actual shipped files, not just synthetic ones: a regenerated
    // asset that this loader cannot read would be silent in game.
    const char* paths[] = {
        "assets/sounds/weapon_fire.wav",
        "assets/sounds/explosion.wav",
        "assets/sounds/graze.wav",
        "assets/sounds/music_level.wav",
    };

    for (const char* path : paths) {
        hu::WavData wav;
        std::string error;
        const bool loaded = hu::loadWav(path, wav, &error);
        INFO(path << ": " << (loaded ? "ok" : error));
        REQUIRE(loaded);
        CHECK(wav.valid());
        CHECK(wav.frameCount() > 0);
    }
}

TEST_CASE("A missing file fails cleanly", "[audio][wav]") {
    hu::WavData wav;
    std::string error;
    CHECK_FALSE(hu::loadWav("assets/sounds/does_not_exist.wav", wav, &error));
    CHECK_FALSE(error.empty());
}
