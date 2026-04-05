#pragma once

#include <atomic>
#include <string>
#include <vector>

// VOD segment 스트리밍용 전역 변수
extern std::vector<std::string> g_vod_segments;
extern std::atomic<bool> g_vod_ready;

namespace chzzk {

struct SwitchPlaybackRequest {
  std::string title;
  std::string url;
  std::string referer;
  std::string http_header_fields;
  std::string channel_id;
  std::string channel_name;
  std::string chat_channel_id;
};

bool run_switch_player(const SwitchPlaybackRequest& request, std::string& error);

}  // namespace chzzk
