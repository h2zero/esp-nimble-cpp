#include "../../examples/L2CAP/L2CAP_Bench/main/bench_protocol.hpp"
#include <cassert>

int main() {
    bench::Parser parser;
    std::vector<bench::Frame> frames;
    auto emit = [&](bench::Frame frame) { frames.push_back(std::move(frame)); };
    const auto hello = bench::encode(bench::hello, {1, 'S', '3'});
    // A byte stream can split every header and payload byte.
    for (auto byte : hello) assert(parser.feed({byte}, emit));
    assert(frames.size() == 1 && frames[0].payload == std::vector<uint8_t>({1, 'S', '3'}));
    auto combined = hello; combined.insert(combined.end(), hello.begin(), hello.end());
    assert(parser.feed(combined, emit)); assert(frames.size() == 3);
    parser.reset(); assert(!parser.feed({0xB7, 3, 0x13, 0x85}, emit));
    assert(!parser.feed(hello, emit)); // A malformed stream stays rejected until reconnect/reset.
    parser.reset(); assert(parser.feed(hello, emit));
    bench::Config config;
    assert(config.parse({2, 0, 8, 0x10, 0, 0, 0, 0, 64}));
    assert(config.request == 8 && config.response == 4096 && config.count == 64);
    assert(!config.parse({1, 0, 8, 0x10, 0, 0, 0, 0, 64}));
    std::vector<uint8_t> numbers; bench::put32(numbers, 0xAABBCCDD);
    assert(numbers == std::vector<uint8_t>({0xAA, 0xBB, 0xCC, 0xDD}));
    assert(bench::u32(numbers.data()) == 0xAABBCCDD);
    assert(bench::pattern(0x01020304, 0xAABBCCDD, 0) == 0xE1);
    for (uint8_t mode : {3, 4, 5}) {
        bench::Config streaming{mode, 4096, 4096, 64};
        assert(config.parse(streaming.encode()));
    }
    bench::StreamReceiver receiver; receiver.reset(0xAABBCCDD, 2, 64);
    auto p0 = bench::streamPayload(0, 0xAABBCCDD, 64);
    auto p1 = bench::streamPayload(1, 0xAABBCCDD, 64);
    assert(receiver.receive(p0, 0x100000001));
    assert(receiver.receive(p1, 0x100000002));
    std::vector<uint8_t> end; bench::put32(end, 0xAABBCCDD); bench::put32(end, 2);
    assert(receiver.end(end));
    bench::StreamSummary summary; assert(summary.parse(receiver.summary.encode()));
    assert(summary.matches(0xAABBCCDD, 2, 64) && summary.firstUs == 0x100000001);
    assert(!receiver.receive(p1, 0x100000003)); // Data after END cannot pass.
    for (const auto& bad : {p0, bench::streamPayload(1, 42, 64), bench::streamPayload(1, 0xAABBCCDD, 65)}) {
        receiver.reset(0xAABBCCDD, 2, 64); assert(receiver.receive(p0, 100));
        assert(!receiver.receive(bad, 200)); assert(!receiver.end(end));
    }
    receiver.reset(0xAABBCCDD, 2, 64); assert(receiver.receive(p1, 100) == false); // Gap.
    receiver.reset(0xAABBCCDD, 2, 64); p0.back() ^= 1;
    assert(!receiver.receive(p0, 100));
    assert(!summary.parse({1, 2, 3}));
}
