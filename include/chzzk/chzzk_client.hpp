#pragma once

#include <optional>
#include <string>

#include "chzzk/http_client.hpp"
#include "chzzk/models.hpp"

namespace chzzk {

class ChzzkClient {
public:
  explicit ChzzkClient(HttpClient& http_client);

  std::optional<LiveListResponse> get_popular_lives(int size = 20);
  std::optional<LiveDetail> get_live_detail(const std::string& channel_id);
  std::optional<ResolvedPlayback> resolve_playback(const LiveDetail& detail,
                                                   const PlaybackPreference& preference);

private:
  HttpClient& http_client_;
};

}  // namespace chzzk
