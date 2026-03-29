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

// 카테고리
struct CategoryInfo {
  std::string category_type;
  std::string category_id;
  std::string category_value;
  std::string poster_image_url;
  int concurrent_user_count = 0;
};

struct CategoryListResponse {
  std::vector<CategoryInfo> data;
};

// 검색
struct SearchChannelResult {
  Channel channel;
  bool is_live = false;
  int concurrent_user_count = 0;
  std::string live_title;
  std::string live_category_value;
};

struct SearchChannelResponse {
  std::vector<SearchChannelResult> data;
  int total_count = 0;
};

// VOD
struct VodInfo {
  int video_no = 0;
  std::string video_id;
  std::string video_title;
  std::string thumbnail_image_url;
  int duration = 0;
  int read_count = 0;
  std::string video_category_value;
  std::string publish_date;
  Channel channel;
};

struct VodListResponse {
  std::vector<VodInfo> data;
};

struct VodDetail {
  int video_no = 0;
  std::string video_id;
  std::string video_title;
  std::string in_key;
  int duration = 0;
  Channel channel;
};

// 유틸
inline std::string format_duration(int seconds) {
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  char buf[32];
  if (h > 0)
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
  else
    snprintf(buf, sizeof(buf), "%d:%02d", m, s);
  return buf;
}

// 유틸
inline std::string format_viewer_count(int count) {
  if (count >= 10000) {
    int major = count / 10000;
    int minor = (count % 10000) / 1000;
    if (minor > 0)
      return std::to_string(major) + "." + std::to_string(minor) + "만";
    return std::to_string(major) + "만";
  }
  if (count >= 1000) {
    return std::to_string(count / 1000) + "," +
           std::string(3 - std::to_string(count % 1000).length(), '0') +
           std::to_string(count % 1000);
  }
  return std::to_string(count);
}

}  // namespace chzzk
