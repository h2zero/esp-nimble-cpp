#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bench {
constexpr uint8_t magic = 0xB7;
constexpr size_t maxPayload = 4996;
constexpr uint8_t hello = 1, config = 2, ping = 3, abort = 0x7F;
constexpr uint8_t helloReply = 0x81, configReply = 0x82, pingReply = 0x83;
constexpr uint8_t version = 2;
constexpr uint8_t stream = 4, streamEnd = 5, streamStart = 6, streamFinish = 7;
constexpr uint8_t streamEndReply = 0x85, streamStartReply = 0x86, streamFinishReply = 0x87;
struct Frame { uint8_t type; std::vector<uint8_t> payload; int64_t receivedUs = 0; uint32_t generation = 0; };
inline uint32_t u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline void put32(std::vector<uint8_t>& p, uint32_t v) {
    p.push_back(v >> 24); p.push_back(v >> 16); p.push_back(v >> 8); p.push_back(v);
}
inline uint64_t u64(const uint8_t* p) { return (uint64_t(u32(p)) << 32) | u32(p + 4); }
inline void put64(std::vector<uint8_t>& p, uint64_t v) { put32(p, v >> 32); put32(p, uint32_t(v)); }
inline uint8_t pattern(uint32_t sequence, uint32_t token, size_t offset) {
    return static_cast<uint8_t>(sequence + token + offset);
}
inline std::vector<uint8_t> encode(uint8_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> result{magic, type, uint8_t(payload.size() >> 8), uint8_t(payload.size())};
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
class Parser {
    std::vector<uint8_t> bytes;
    bool failed = false;
public:
    void reset() { bytes.clear(); failed = false; }
    template<typename Emit> bool feed(const std::vector<uint8_t>& incoming, Emit emit) {
        if (failed) return false;
        // Process incrementally: a read may contain multiple complete frames.
        for (auto byte : incoming) {
            bytes.push_back(byte);
            if (bytes.size() == 1 && byte != magic) { failed = true; return false; }
            if (bytes.size() < 4) continue;
            const size_t length = (size_t(bytes[2]) << 8) | bytes[3];
            if (length > maxPayload) { failed = true; return false; }
            if (bytes.size() == length + 4) {
                emit(Frame{bytes[1], {bytes.begin() + 4, bytes.end()}});
                bytes.clear();
            }
        }
        return true;
    }
};
struct Config {
    uint8_t pattern = 0;
    uint16_t request = 0, response = 0;
    uint32_t count = 0;
    bool parse(const std::vector<uint8_t>& p) {
        if (p.size() != 9) return false;
        Config candidate{p[0], uint16_t((uint16_t(p[1]) << 8) | p[2]),
                         uint16_t((uint16_t(p[3]) << 8) | p[4]), u32(p.data() + 5)};
        if (candidate.pattern < 1 || candidate.pattern > 5 || candidate.request < 8 || candidate.request > maxPayload ||
            candidate.response < 12 || candidate.response > maxPayload || candidate.count == 0 || candidate.count > 4096 ||
            (candidate.pattern == 1 && candidate.request != candidate.response)) return false;
        *this = candidate;
        return true;
    }
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> p{pattern, uint8_t(request >> 8), uint8_t(request), uint8_t(response >> 8), uint8_t(response)};
        put32(p, count); return p;
    }
};

// These are application stream frames, independent of SDU boundaries.
inline std::vector<uint8_t> streamPayload(uint32_t seq, uint32_t token, uint16_t size) {
    std::vector<uint8_t> p; p.reserve(size); put32(p, seq); put32(p, token);
    for (size_t i = 8; i < size; ++i) p.push_back(pattern(seq, token, i - 8));
    return p;
}
struct StreamSummary {
    uint32_t token = 0, frames = 0;
    uint64_t bytes = 0, firstUs = 0, lastUs = 0;
    uint32_t errors = 0;
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> p; put32(p, token); put32(p, frames); put64(p, bytes);
        put64(p, firstUs); put64(p, lastUs); put32(p, errors); return p;
    }
    bool parse(const std::vector<uint8_t>& p) {
        if (p.size() != 36) return false;
        *this = {u32(p.data()), u32(p.data() + 4), u64(p.data() + 8),
                 u64(p.data() + 16), u64(p.data() + 24), u32(p.data() + 32)};
        return firstUs <= lastUs;
    }
    bool matches(uint32_t expectedToken, uint32_t count, uint16_t size) const {
        return token == expectedToken && frames == count && bytes == uint64_t(count) * size &&
               errors == 0 && firstUs <= lastUs;
    }
};
struct StreamReceiver {
    StreamSummary summary;
    uint32_t count = 0;
    uint16_t size = 0;
    bool ended = false;
    std::vector<uint64_t> arrivals;
    void reset(uint32_t token, uint32_t expectedCount, uint16_t expectedSize) {
        summary = {}; summary.token = token; count = expectedCount; size = expectedSize;
        ended = false; arrivals.clear(); arrivals.reserve(expectedCount);
    }
    bool receive(const std::vector<uint8_t>& p, uint64_t now) {
        bool valid = !ended && summary.frames < count && p.size() == size && p.size() >= 8 &&
                     u32(p.data()) == summary.frames && u32(p.data() + 4) == summary.token;
        for (size_t i = 8; valid && i < p.size(); ++i)
            valid = p[i] == pattern(summary.frames, summary.token, i - 8);
        if (!valid) { ++summary.errors; return false; }
        if (!summary.frames) summary.firstUs = now;
        summary.lastUs = now; ++summary.frames; summary.bytes += p.size(); arrivals.push_back(now);
        return true;
    }
    bool end(const std::vector<uint8_t>& p) {
        const bool valid = !ended && p.size() == 8 && u32(p.data()) == summary.token &&
                           u32(p.data() + 4) == count && summary.matches(summary.token, count, size);
        ended = true;
        if (!valid) ++summary.errors;
        return valid;
    }
};
} // namespace bench
