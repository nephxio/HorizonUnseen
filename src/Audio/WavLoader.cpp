#include "Audio/WavLoader.h"

#include <cstring>
#include <fstream>

namespace hu {

namespace {

// Chunk sizes come straight from the file, so every read is bounds-checked
// against the buffer rather than trusted. A truncated wav is a plausible
// accident (an interrupted asset generation) and must not walk off the end.
bool readU32(const std::uint8_t* data, std::size_t size, std::size_t offset, std::uint32_t& out) {
    if (offset + 4 > size) {
        return false;
    }
    out = static_cast<std::uint32_t>(data[offset]) |
          (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
          (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
          (static_cast<std::uint32_t>(data[offset + 3]) << 24);
    return true;
}

bool readU16(const std::uint8_t* data, std::size_t size, std::size_t offset, std::uint16_t& out) {
    if (offset + 2 > size) {
        return false;
    }
    out = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset]) |
                                     (static_cast<std::uint16_t>(data[offset + 1]) << 8));
    return true;
}

bool tagEquals(const std::uint8_t* data, std::size_t size, std::size_t offset, const char* tag) {
    return offset + 4 <= size && std::memcmp(data + offset, tag, 4) == 0;
}

void fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
}

constexpr std::uint16_t kFormatPcm = 1;

} // namespace

bool parseWav(const std::uint8_t* data, std::size_t size, WavData& out, std::string* error) {
    if (data == nullptr || size < 12) {
        fail(error, "file is too small to be a wav");
        return false;
    }
    if (!tagEquals(data, size, 0, "RIFF") || !tagEquals(data, size, 8, "WAVE")) {
        fail(error, "not a RIFF/WAVE file");
        return false;
    }

    WavData result;
    bool haveFormat = false;
    bool haveData = false;

    // Walk the chunk list rather than assuming fmt is immediately followed by
    // data: writers are entitled to insert LIST/fact chunks between them, and
    // some do.
    std::size_t offset = 12;
    while (offset + 8 <= size) {
        std::uint32_t chunkSize = 0;
        if (!readU32(data, size, offset + 4, chunkSize)) {
            break;
        }
        const std::size_t body = offset + 8;

        if (tagEquals(data, size, offset, "fmt ")) {
            std::uint16_t format = 0;
            std::uint16_t channels = 0;
            std::uint16_t bits = 0;
            std::uint32_t sampleRate = 0;
            if (!readU16(data, size, body + 0, format) ||
                !readU16(data, size, body + 2, channels) ||
                !readU32(data, size, body + 4, sampleRate) ||
                !readU16(data, size, body + 14, bits)) {
                fail(error, "fmt chunk is truncated");
                return false;
            }
            if (format != kFormatPcm) {
                fail(error, "not uncompressed PCM");
                return false;
            }
            if (bits != 16) {
                fail(error, "only 16-bit samples are supported");
                return false;
            }
            if (channels != 1 && channels != 2) {
                fail(error, "only mono and stereo are supported");
                return false;
            }
            result.channels = channels;
            result.bitsPerSample = bits;
            result.sampleRate = static_cast<int>(sampleRate);
            haveFormat = true;
        } else if (tagEquals(data, size, offset, "data")) {
            // Clamp rather than reject: a file whose header promises more than
            // it contains is still playable up to the point it was truncated,
            // and that is a better failure than silence.
            const std::size_t available = size - body;
            const std::size_t length = chunkSize <= available ? chunkSize : available;
            result.samples.assign(data + body, data + body + length);
            haveData = true;
        }

        // Chunks are word-aligned; an odd size is followed by a pad byte.
        offset = body + chunkSize + (chunkSize & 1u);
    }

    if (!haveFormat) {
        fail(error, "no fmt chunk");
        return false;
    }
    if (!haveData || result.samples.empty()) {
        fail(error, "no data chunk");
        return false;
    }

    // Drop a trailing partial frame so consumers can divide cleanly.
    const std::size_t bytesPerFrame =
        static_cast<std::size_t>(result.channels) * (result.bitsPerSample / 8);
    result.samples.resize(result.samples.size() - (result.samples.size() % bytesPerFrame));
    if (result.samples.empty()) {
        fail(error, "data chunk holds less than one frame");
        return false;
    }

    out = std::move(result);
    return true;
}

bool loadWav(const std::string& path, WavData& out, std::string* error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        fail(error, "could not open file");
        return false;
    }

    const std::streamoff size = file.tellg();
    if (size <= 0) {
        fail(error, "file is empty");
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        fail(error, "could not read file");
        return false;
    }

    return parseWav(bytes.data(), bytes.size(), out, error);
}

} // namespace hu
