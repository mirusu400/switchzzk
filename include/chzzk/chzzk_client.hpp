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
  std::optional<CategoryListResponse> get_live_categories(int size = 30);
  std::optional<LiveListResponse> get_category_lives(const std::string& category_type,
                                                      const std::string& category_id,
                                                      int size = 20);
  std::optional<SearchChannelResponse> search_channels(const std::string& keyword,
                                                        int size = 20, int offset = 0);

private:
  HttpClient& http_client_;
};

}  // namespace chzzk
