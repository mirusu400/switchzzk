#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chzzk {

inline constexpr const char* kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/134.0.0.0 Whale/4.31.304.16 Safari/537.36";
inline constexpr const char* kServiceBaseUrl =
    "https://api.chzzk.naver.com/service";

struct Channel {
  std::string channel_id;
  std::string channel_name;
  std::string channel_image_url;
  bool verified_mark = false;
};

struct LiveInfo {
  std::int64_t live_id = 0;
  std::string live_title;
  std::string live_image_url;
  int concurrent_user_count = 0;
  std::string live_category;
  std::string live_category_value;
  Channel channel;
};

struct LiveListResponse {
  std::vector<LiveInfo> data;
  std::optional<int> next_concurrent_user_count;
  std::optional<std::int64_t> next_live_id;
};

struct EncodingTrack {
  std::string id;
  int width = 0;
  int height = 0;
  std::string path;
};

struct Media {
  std::string media_id;
  std::string protocol;
  std::string path;
  std::string latency;
  std::vector<EncodingTrack> encoding_tracks;
};

struct LiveDetail {
  std::int64_t live_id = 0;
  std::string live_title;
  int concurrent_user_count = 0;
  bool adult = false;
  bool kr_only_viewing = false;
  std::string chat_channel_id;
  std::string live_category;
  std::string live_category_value;
  Channel channel;
  std::vector<Media> media;
};

struct PlaybackPreference {
  bool prefer_low_latency = false;
  int max_height = 720;
};

struct PlaybackResolution {
  int width = 0;
  int height = 0;
};

struct ResolvedPlayback {
  std::string master_url;
  std::string selected_url;
  std::string media_id;
  PlaybackResolution resolution;
};

}  // namespace chzzk
