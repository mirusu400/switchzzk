#pragma once

#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include "chzzk/models.hpp"

namespace chzzk {

inline constexpr int kMaxRecentChannels = 10;
inline constexpr const char* kRecentChannelsPath = "sdmc:/switch/switchzzk_recent.json";

// Minimal JSON read/write without a library.
// Format: [{"id":"...","name":"...","ts":123}, ...]

inline std::vector<RecentChannel> load_recent_channels() {
    std::vector<RecentChannel> result;
    FILE* f = fopen(kRecentChannelsPath, "r");
    if (!f) return result;

    // Read entire file
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 65536) { fclose(f); return result; }

    std::string buf(sz, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);

    // Simple parser: find each {"id":"...","name":"...","ts":...}
    size_t pos = 0;
    while (pos < buf.size()) {
        auto obj_start = buf.find('{', pos);
        if (obj_start == std::string::npos) break;
        auto obj_end = buf.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = buf.substr(obj_start, obj_end - obj_start + 1);
        RecentChannel ch;

        // Extract "id":"value"
        auto extract = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto p = obj.find(needle);
            if (p == std::string::npos) return "";
            p += needle.size();
            auto e = obj.find('"', p);
            if (e == std::string::npos) return "";
            return obj.substr(p, e - p);
        };

        ch.channel_id = extract("id");
        ch.channel_name = extract("name");

        // Extract "ts":number
        auto ts_pos = obj.find("\"ts\":");
        if (ts_pos != std::string::npos) {
            ts_pos += 5;
            ch.timestamp = std::strtoll(obj.c_str() + ts_pos, nullptr, 10);
        }

        if (!ch.channel_id.empty())
            result.push_back(std::move(ch));

        pos = obj_end + 1;
    }
    return result;
}

inline void save_recent_channels(const std::vector<RecentChannel>& channels) {
    FILE* f = fopen(kRecentChannelsPath, "w");
    if (!f) return;

    fprintf(f, "[");
    for (size_t i = 0; i < channels.size(); i++) {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "\n{\"id\":\"%s\",\"name\":\"%s\",\"ts\":%lld}",
                channels[i].channel_id.c_str(),
                channels[i].channel_name.c_str(),
                (long long)channels[i].timestamp);
    }
    fprintf(f, "\n]\n");
    fclose(f);
}

inline void add_recent_channel(const std::string& channel_id, const std::string& channel_name) {
    auto channels = load_recent_channels();

    // Remove existing entry for this channel
    channels.erase(
        std::remove_if(channels.begin(), channels.end(),
            [&](const RecentChannel& c) { return c.channel_id == channel_id; }),
        channels.end());

    // Insert at front
    RecentChannel ch;
    ch.channel_id = channel_id;
    ch.channel_name = channel_name;
    ch.timestamp = static_cast<std::int64_t>(std::time(nullptr));
    channels.insert(channels.begin(), ch);

    // Trim to max
    if ((int)channels.size() > kMaxRecentChannels)
        channels.resize(kMaxRecentChannels);

    save_recent_channels(channels);
}

}  // namespace chzzk
