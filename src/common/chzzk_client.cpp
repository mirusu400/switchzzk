#include "chzzk/chzzk_client.hpp"

#include <algorithm>
#include <sstream>

#include "chzzk/m3u8.hpp"
#include "nlohmann/json.hpp"

namespace chzzk {
namespace {

using nlohmann::json;

const json& unwrap_content(const json& root) {
  if (root.is_object() && root.contains("content")) {
    return root.at("content");
  }
  return root;
}

std::string get_string(const json& node, const char* key) {
  if (!node.contains(key) || node.at(key).is_null()) {
    return {};
  }
  return node.at(key).get<std::string>();
}

bool get_bool(const json& node, const char* key, bool fallback = false) {
  if (!node.contains(key) || node.at(key).is_null()) {
    return fallback;
  }
  return node.at(key).get<bool>();
}

int get_int(const json& node, const char* key, int fallback = 0) {
  if (!node.contains(key) || node.at(key).is_null()) {
    return fallback;
  }
  return node.at(key).get<int>();
}

std::int64_t get_i64(const json& node, const char* key, std::int64_t fallback = 0) {
  if (!node.contains(key) || node.at(key).is_null()) {
    return fallback;
  }
  return node.at(key).get<std::int64_t>();
}

Channel parse_channel(const json& node) {
  Channel channel;
  channel.channel_id = get_string(node, "channelId");
  channel.channel_name = get_string(node, "channelName");
  channel.channel_image_url = get_string(node, "channelImageUrl");
  channel.verified_mark = get_bool(node, "verifiedMark", false);
  return channel;
}

LiveInfo parse_live_info(const json& node) {
  LiveInfo info;
  info.live_id = get_i64(node, "liveId");
  info.live_title = get_string(node, "liveTitle");
  info.live_image_url = get_string(node, "liveImageUrl");
  info.concurrent_user_count = get_int(node, "concurrentUserCount", 0);
  info.live_category = get_string(node, "liveCategory");
  info.live_category_value = get_string(node, "liveCategoryValue");
  if (node.contains("channel") && node.at("channel").is_object()) {
    info.channel = parse_channel(node.at("channel"));
  }
  return info;
}

std::vector<Media> parse_media_list(const json& playback_json) {
  std::vector<Media> media_list;
  if (!playback_json.contains("media") || !playback_json.at("media").is_array()) {
    return media_list;
  }

  for (const auto& media_node : playback_json.at("media")) {
    Media media;
    media.media_id = get_string(media_node, "mediaId");
    media.protocol = get_string(media_node, "protocol");
    media.path = get_string(media_node, "path");
    media.latency = get_string(media_node, "latency");

    if (media_node.contains("encodingTrack") && media_node.at("encodingTrack").is_array()) {
      for (const auto& track_node : media_node.at("encodingTrack")) {
        EncodingTrack track;
        track.id = get_string(track_node, "encodingTrackId");
        track.width = get_int(track_node, "videoWidth");
        track.height = get_int(track_node, "videoHeight");
        track.path = get_string(track_node, "path");
        media.encoding_tracks.push_back(track);
      }
    }

    media_list.push_back(std::move(media));
  }

  return media_list;
}

std::optional<Media> choose_media(const std::vector<Media>& media_list,
                                  bool prefer_low_latency) {
  const std::string preferred_id = prefer_low_latency ? "LLHLS" : "HLS";
  auto preferred_it = std::find_if(media_list.begin(), media_list.end(),
                                   [&](const Media& media) {
                                     return media.media_id == preferred_id;
                                   });
  if (preferred_it != media_list.end()) {
    return *preferred_it;
  }

  auto fallback_it = std::find_if(media_list.begin(), media_list.end(),
                                  [](const Media& media) {
                                    return media.media_id == "HLS" || media.media_id == "LLHLS";
                                  });
  if (fallback_it != media_list.end()) {
    return *fallback_it;
  }

  if (!media_list.empty()) {
    return media_list.front();
  }
  return std::nullopt;
}

}  // namespace

ChzzkClient::ChzzkClient(HttpClient& http_client) : http_client_(http_client) {}

std::optional<LiveListResponse> ChzzkClient::get_popular_lives(
    int size, std::optional<int> cursor_count, std::optional<std::int64_t> cursor_id) {
  std::string url = std::string(kServiceBaseUrl) + "/v1/lives?size=" +
                    std::to_string(size) + "&sortType=POPULAR";
  if (cursor_count.has_value())
    url += "&concurrentUserCount=" + std::to_string(*cursor_count);
  if (cursor_id.has_value())
    url += "&liveId=" + std::to_string(*cursor_id);
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) {
    return std::nullopt;
  }

  const auto& content = unwrap_content(root);
  if (!content.is_object() || !content.contains("data") || !content.at("data").is_array()) {
    return std::nullopt;
  }

  LiveListResponse response;
  for (const auto& item : content.at("data")) {
    response.data.push_back(parse_live_info(item));
  }

  if (content.contains("page") && content.at("page").is_object()) {
    const auto& page = content.at("page");
    if (page.contains("next") && page.at("next").is_object()) {
      const auto& next = page.at("next");
      if (next.contains("concurrentUserCount")) {
        response.next_concurrent_user_count = next.at("concurrentUserCount").get<int>();
      }
      if (next.contains("liveId")) {
        response.next_live_id = next.at("liveId").get<std::int64_t>();
      }
    }
  }

  return response;
}

std::optional<LiveDetail> ChzzkClient::get_live_detail(const std::string& channel_id) {
  const std::string url = std::string(kServiceBaseUrl) + "/v3.1/channels/" +
                          channel_id + "/live-detail";
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) {
    return std::nullopt;
  }

  const auto& content = unwrap_content(root);
  if (!content.is_object()) {
    return std::nullopt;
  }

  LiveDetail detail;
  detail.live_id = get_i64(content, "liveId");
  detail.live_title = get_string(content, "liveTitle");
  detail.concurrent_user_count = get_int(content, "concurrentUserCount");
  detail.adult = get_bool(content, "adult");
  detail.kr_only_viewing = get_bool(content, "krOnlyViewing");
  detail.chat_channel_id = get_string(content, "chatChannelId");
  detail.live_category = get_string(content, "liveCategory");
  detail.live_category_value = get_string(content, "liveCategoryValue");

  if (content.contains("channel") && content.at("channel").is_object()) {
    detail.channel = parse_channel(content.at("channel"));
  }

  if (content.contains("livePlaybackJson")) {
    json playback_json;
    if (content.at("livePlaybackJson").is_string()) {
      playback_json = json::parse(content.at("livePlaybackJson").get<std::string>(), nullptr, false);
    } else if (content.at("livePlaybackJson").is_object()) {
      playback_json = content.at("livePlaybackJson");
    }

    if (!playback_json.is_discarded() && playback_json.is_object()) {
      detail.media = parse_media_list(playback_json);
    }
  }

  return detail;
}

std::optional<ResolvedPlayback> ChzzkClient::resolve_playback(
    const LiveDetail& detail,
    const PlaybackPreference& preference) {
  auto media = choose_media(detail.media, preference.prefer_low_latency);
  if (!media.has_value() || media->path.empty()) {
    return std::nullopt;
  }

  ResolvedPlayback resolved;
  resolved.master_url = media->path;
  resolved.selected_url = media->path;
  resolved.media_id = media->media_id;

  auto playlist = http_client_.get(media->path, {{"User-Agent", kDefaultUserAgent}});
  if (!playlist.has_value()) {
    return resolved;
  }

  const auto variants = parse_variant_playlist(*playlist, media->path);
  const auto selected_variant = choose_variant(variants, preference.max_height);
  if (!selected_variant.has_value()) {
    return resolved;
  }

  resolved.selected_url = selected_variant->uri;
  resolved.resolution = selected_variant->resolution;
  return resolved;
}

std::optional<CategoryListResponse> ChzzkClient::get_live_categories(int size) {
  const std::string url = std::string(kServiceBaseUrl) + "/v1/categories/live?size=" +
                          std::to_string(size);
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) return std::nullopt;

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) return std::nullopt;

  const auto& content = unwrap_content(root);
  if (!content.is_object() || !content.contains("data") || !content.at("data").is_array())
    return std::nullopt;

  CategoryListResponse response;
  for (const auto& item : content.at("data")) {
    CategoryInfo cat;
    cat.category_type = get_string(item, "categoryType");
    cat.category_id = get_string(item, "categoryId");
    cat.category_value = get_string(item, "categoryValue");
    cat.poster_image_url = get_string(item, "posterImageUrl");
    cat.concurrent_user_count = get_int(item, "concurrentUserCount", 0);
    response.data.push_back(std::move(cat));
  }
  return response;
}

std::optional<LiveListResponse> ChzzkClient::get_category_lives(
    const std::string& category_type,
    const std::string& category_id,
    int size) {
  const std::string url = std::string(kServiceBaseUrl) + "/v2/categories/" +
                          category_type + "/" + category_id + "/lives?size=" +
                          std::to_string(size);
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) return std::nullopt;

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) return std::nullopt;

  const auto& content = unwrap_content(root);
  if (!content.is_object() || !content.contains("data") || !content.at("data").is_array())
    return std::nullopt;

  LiveListResponse response;
  for (const auto& item : content.at("data")) {
    response.data.push_back(parse_live_info(item));
  }
  return response;
}

std::optional<SearchChannelResponse> ChzzkClient::search_channels(
    const std::string& keyword, int size, int offset) {
  // URL-encode keyword (minimal: spaces → +)
  std::string encoded;
  for (char c : keyword) {
    if (c == ' ') encoded += '+';
    else encoded += c;
  }
  const std::string url = std::string(kServiceBaseUrl) + "/v1/search/channels?keyword=" +
                          encoded + "&size=" + std::to_string(size) +
                          "&offset=" + std::to_string(offset);
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) return std::nullopt;

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) return std::nullopt;

  const auto& content = unwrap_content(root);
  if (!content.is_object() || !content.contains("data") || !content.at("data").is_array())
    return std::nullopt;

  SearchChannelResponse response;
  for (const auto& item : content.at("data")) {
    SearchChannelResult result;
    if (item.contains("channel") && item.at("channel").is_object()) {
      result.channel = parse_channel(item.at("channel"));
    }
    if (item.contains("live") && item.at("live").is_object()) {
      const auto& live = item.at("live");
      result.is_live = true;
      result.concurrent_user_count = get_int(live, "concurrentUserCount", 0);
      result.live_title = get_string(live, "liveTitle");
      result.live_category_value = get_string(live, "liveCategoryValue");
    }
    response.data.push_back(std::move(result));
  }
  if (content.contains("size")) {
    response.total_count = get_int(content, "size");
  }
  return response;
}

std::optional<VodListResponse> ChzzkClient::get_popular_vods(int size) {
  const std::string url = std::string(kServiceBaseUrl) + "/v1/home/videos?size=" +
                          std::to_string(size) + "&sortType=POPULAR";
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) return std::nullopt;

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) return std::nullopt;

  const auto& content = unwrap_content(root);
  if (!content.is_object() || !content.contains("data") || !content.at("data").is_array())
    return std::nullopt;

  VodListResponse response;
  for (const auto& item : content.at("data")) {
    VodInfo vod;
    vod.video_no = get_int(item, "videoNo");
    vod.video_id = get_string(item, "videoId");
    vod.video_title = get_string(item, "videoTitle");
    vod.thumbnail_image_url = get_string(item, "thumbnailImageUrl");
    vod.duration = get_int(item, "duration");
    vod.read_count = get_int(item, "readCount");
    vod.video_category_value = get_string(item, "videoCategoryValue");
    vod.publish_date = get_string(item, "publishDate");
    if (item.contains("channel") && item.at("channel").is_object())
      vod.channel = parse_channel(item.at("channel"));
    response.data.push_back(std::move(vod));
  }
  return response;
}

std::optional<VodDetail> ChzzkClient::get_vod_detail(int video_no) {
  const std::string url = std::string(kServiceBaseUrl) + "/v3/videos/" +
                          std::to_string(video_no);
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) return std::nullopt;

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) return std::nullopt;

  const auto& content = unwrap_content(root);
  if (!content.is_object()) return std::nullopt;

  VodDetail detail;
  detail.video_no = get_int(content, "videoNo");
  detail.video_id = get_string(content, "videoId");
  detail.video_title = get_string(content, "videoTitle");
  detail.in_key = get_string(content, "inKey");
  detail.duration = get_int(content, "duration");
  if (content.contains("channel") && content.at("channel").is_object())
    detail.channel = parse_channel(content.at("channel"));
  return detail;
}

std::optional<std::string> ChzzkClient::get_vod_playback_url(const VodDetail& vod) {
  if (vod.video_id.empty() || vod.in_key.empty()) return std::nullopt;

  const std::string url =
      "https://apis.naver.com/neonplayer/vodplay/v2/playback/" + vod.video_id +
      "?key=" + vod.in_key + "&sid=2099&devt=html5_pc&st=5&lc=ko_KR&cpl=ko_KR";
  auto payload = http_client_.get(url, {{"User-Agent", kDefaultUserAgent}});
  if (!payload.has_value()) return std::nullopt;

  const auto root = json::parse(*payload, nullptr, false);
  if (root.is_discarded()) return std::nullopt;

  // period[0].adaptationSet[0].otherAttributes.m3u
  if (!root.contains("period") || !root.at("period").is_array()) return std::nullopt;
  for (const auto& period : root.at("period")) {
    if (!period.contains("adaptationSet") || !period.at("adaptationSet").is_array()) continue;
    for (const auto& as : period.at("adaptationSet")) {
      if (!as.contains("otherAttributes")) continue;
      const auto& attrs = as.at("otherAttributes");
      std::string m3u = get_string(attrs, "m3u");
      if (!m3u.empty()) return m3u;
    }
  }
  return std::nullopt;
}

}  // namespace chzzk
