// Included by main.cpp inside the benchmark namespace. No product protocol code.
bool sendStream(NimBLEL2CAPChannel* channel, uint32_t generation, uint32_t token,
                uint32_t count, uint16_t size, std::vector<uint32_t>* submits = nullptr) {
    for (uint32_t seq = 0; seq < count; ++seq) {
        if (!connected.load() || generation != connectionGeneration.load()) return false;
        const auto payload = bench::encode(bench::stream, bench::streamPayload(seq, token, size));
        const auto started = esp_timer_get_time();
        if (!channel->write(payload)) return false;
        if (submits) submits->push_back(uint32_t(esp_timer_get_time() - started));
    }
    std::vector<uint8_t> end; bench::put32(end, token); bench::put32(end, count);
    return generation == connectionGeneration.load() && channel->write(bench::encode(bench::streamEnd, end));
}

void addSummary(cJSON* parent, const char* key, const bench::StreamSummary& summary) {
    auto object = cJSON_CreateObject();
    number(object, "token", summary.token); number(object, "frames", summary.frames);
    number(object, "bytes", summary.bytes); number(object, "first_us", summary.firstUs);
    number(object, "last_us", summary.lastUs); number(object, "errors", summary.errors);
    cJSON_AddItemToObject(parent, key, object);
}

int runStream(const bench::Config& config, const char* name) {
    const bool send = config.pattern != 4, receive = config.pattern != 3;
    const uint32_t token = esp_random(), generation = connectionGeneration.load();
    const auto faults = callbackFaults.load();
    {
        std::lock_guard<std::mutex> lock(streamMutex);
        streamReceiver.reset(token, receive ? config.count : 0, config.response);
    }
    std::vector<uint32_t> submits; if (send) submits.reserve(config.count);
    auto begin = cJSON_CreateObject();
    cJSON_AddStringToObject(begin, "pattern", name);
    number(begin, "count", config.count); number(begin, "request_size", config.request);
    number(begin, "response_size", config.response); number(begin, "token", token); emit("run-start", begin);
    std::vector<uint8_t> tokenPayload; bench::put32(tokenPayload, token);
    const auto started = esp_timer_get_time();
    auto ready = exchange(bench::streamStart, tokenPayload, bench::streamStartReply);
    bool success = ready && ready->payload == tokenPayload;
    auto channel = activeChannel.load();
    const auto txStarted = esp_timer_get_time();
    if (success && send) success = channel && sendStream(channel, generation, token, config.count, config.request, &submits);
    const auto txSubmitted = esp_timer_get_time();
    bool rxEnded = !receive, txConfirmed = !send;
    bench::StreamSummary peerSummary;
    const auto waitStarted = esp_timer_get_time();
    while (success && (!rxEnded || !txConfirmed)) {
        bench::Frame* raw = nullptr;
        if (xQueueReceive(replies, &raw, pdMS_TO_TICKS(100)) != pdTRUE) {
            uint64_t progress;
            { std::lock_guard<std::mutex> lock(streamMutex); progress = streamReceiver.summary.lastUs; }
            // A benchmark watchdog, not an ECU response timer: valid RX data
            // refreshes it, including while the two duplex streams differ.
            if (!connected.load() || esp_timer_get_time() - std::max<uint64_t>(waitStarted, progress) > 10000000) success = false;
            continue;
        }
        std::unique_ptr<bench::Frame> frame(raw);
        if (frame->type == bench::streamEnd && !rxEnded) {
            std::vector<uint8_t> reply;
            {
                std::lock_guard<std::mutex> lock(streamMutex);
                success = streamReceiver.end(frame->payload);
                reply = streamReceiver.summary.encode();
            }
            rxEnded = true;
            if (success) success = channel && channel->write(bench::encode(bench::streamEndReply, reply));
        } else if (frame->type == bench::streamEndReply && !txConfirmed) {
            success = peerSummary.parse(frame->payload) && peerSummary.matches(token, config.count, config.request);
            txConfirmed = true;
        } else success = false;
    }
    if (success) {
        auto finished = exchange(bench::streamFinish, tokenPayload, bench::streamFinishReply);
        success = finished && finished->payload == tokenPayload;
    }
    const auto finished = esp_timer_get_time();
    bench::StreamReceiver received;
    { std::lock_guard<std::mutex> lock(streamMutex); received = std::move(streamReceiver); streamReceiver = {}; }
    success = success && callbackFaults.load() == faults && generation == connectionGeneration.load() &&
              (!receive || (received.ended && received.summary.matches(token, config.count, config.response))) &&
              (!send || submits.size() == config.count);
    params();
    auto result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "pattern", name); cJSON_AddBoolToObject(result, "success", success);
    number(result, "completed", success ? config.count : 0); number(result, "token", token);
    number(result, "tx_frames", submits.size()); number(result, "rx_frames", received.summary.frames);
    number(result, "callback_faults", callbackFaults.load() - faults);
    number(result, "elapsed_us", finished - started); number(result, "tx_submit_span_us", txSubmitted - txStarted);
    number(result, "goodput_bytes_per_s", double(submits.size() * (config.request - 8) + received.summary.frames * (config.response - 8)) * 1e6 / (finished - started));
    addSummary(result, "local_rx", received.summary); addSummary(result, "peer_rx", peerSummary);
    cJSON_AddBoolToObject(result, "peer_finish_confirmed", success);
    if (send && peerSummary.frames > 1 && peerSummary.lastUs > peerSummary.firstUs)
        number(result, "tx_arrival_goodput_bytes_per_s", double(peerSummary.frames - 1) * (config.request - 8) * 1e6 / (peerSummary.lastUs - peerSummary.firstUs));
    if (receive && received.summary.frames > 1 && received.summary.lastUs > received.summary.firstUs)
        number(result, "rx_arrival_goodput_bytes_per_s", double(received.summary.frames - 1) * (config.response - 8) * 1e6 / (received.summary.lastUs - received.summary.firstUs));
    emit("run-result", result);
    for (size_t i = 0; i < submits.size(); ++i) {
        auto sample = cJSON_CreateObject(); number(sample, "seq", i); number(sample, "tx_submit_us", submits[i]); emit("stream-tx-sample", sample);
    }
    for (size_t i = 0; i < received.arrivals.size(); ++i) {
        auto sample = cJSON_CreateObject(); number(sample, "seq", i); number(sample, "received_at_us", received.arrivals[i]); emit("stream-rx-sample", sample);
    }
    emit("run-end");
    if (!success) { error("stream data, receipt, finish or watchdog failure"); disconnectClient(); }
    return success ? 0 : 1;
}
