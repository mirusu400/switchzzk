#include "chzzk/m3u8.hpp"

#include <algorithm>
#include <regex>
#include <sstream>

namespace chzzk {
namespace {

std::string trim(const std::string& value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::optional<PlaybackResolution> parse_resolution_from_tag(const std::string& line) {
  static const std::regex kResolutionRegex(R"(RESOLUTION=(\d+)x(\d+))");
  std::smatch match;
  if (!std::regex_search(line, match, kResolutionRegex)) {
    return std::nullopt;
  }

  PlaybackResolution resolution;
  resolution.width = std::stoi(match[1].str());
  resolution.height = std::stoi(match[2].str());
  return resolution;
}

int parse_bandwidth_from_tag(const std::string& line) {
  static const std::regex kBandwidthRegex(R"(BANDWIDTH=(\d+))");
  std::smatch match;
  if (!std::regex_search(line, match, kBandwidthRegex)) {
    return 0;
  }
  return std::stoi(match[1].str());
}

}  // namespace

std::string resolve_relative_url(const std::string& base_url,
                                 const std::string& relative_or_absolute) {
  if (relative_or_absolute.rfind("http://", 0) == 0 ||
      relative_or_absolute.rfind("https://", 0) == 0) {
    return relative_or_absolute;
  }

  if (relative_or_absolute.rfind("//", 0) == 0) {
    const auto scheme_pos = base_url.find("://");
    if (scheme_pos == std::string::npos) {
      return "https:" + relative_or_absolute;
    }
    return base_url.substr(0, scheme_pos) + ":" + relative_or_absolute;
  }

  const auto scheme_pos = base_url.find("://");
  if (scheme_pos == std::string::npos) {
    return relative_or_absolute;
  }

  const auto query_pos = base_url.find_first_of("?#", scheme_pos + 3);
  const std::string base_without_query =
      query_pos == std::string::npos ? base_url : base_url.substr(0, query_pos);

  const auto path_pos = base_without_query.find('/', scheme_pos + 3);
  const std::string origin =
      path_pos == std::string::npos ? base_without_query : base_without_query.substr(0, path_pos);

  if (!relative_or_absolute.empty() && relative_or_absolute.front() == '/') {
    return origin + relative_or_absolute;
  }

  const auto base_dir_end = base_without_query.find_last_of('/');
  std::string result;
  if (base_dir_end == std::string::npos) {
    result = origin + "/" + relative_or_absolute;
  } else {
    result = base_without_query.substr(0, base_dir_end + 1) + relative_or_absolute;
  }

  // variant URL에 query가 없고 base URL에 query가 있으면 붙여준다
  // (Naver VOD CDN의 _lsu_sa_ 인증 토큰 등)
  if (result.find('?') == std::string::npos && query_pos != std::string::npos) {
    result += base_url.substr(query_pos);
  }

  return result;
}

std::vector<VariantStream> parse_variant_playlist(
    const std::string& master_playlist,
    const std::string& master_url) {
  std::vector<VariantStream> variants;
  std::istringstream input(master_playlist);
  std::string line;
  std::optional<PlaybackResolution> pending_resolution;
  int pending_bandwidth = 0;

  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line.rfind("#EXT-X-STREAM-INF:", 0) == 0) {
      pending_resolution = parse_resolution_from_tag(line);
      pending_bandwidth = parse_bandwidth_from_tag(line);
      continue;
    }

    if (!line.empty() && line.front() == '#') {
      continue;
    }

    VariantStream variant;
    variant.uri = resolve_relative_url(master_url, line);
    if (pending_resolution.has_value()) {
      variant.resolution = *pending_resolution;
    } else {
      variant.resolution = PlaybackResolution{.width = 0, .height = 0};
    }
    variant.bandwidth = pending_bandwidth;
    variants.push_back(std::move(variant));
    pending_resolution.reset();
    pending_bandwidth = 0;
  }

  std::sort(variants.begin(), variants.end(), [](const VariantStream& lhs, const VariantStream& rhs) {
    if (lhs.resolution.height == rhs.resolution.height) {
      return lhs.bandwidth < rhs.bandwidth;
    }
    return lhs.resolution.height < rhs.resolution.height;
  });

  return variants;
}

std::optional<VariantStream> choose_variant(
    const std::vector<VariantStream>& variants,
    int max_height) {
  if (variants.empty()) {
    return std::nullopt;
  }

  std::optional<VariantStream> best_under_limit;
  for (const auto& variant : variants) {
    if (variant.resolution.height <= max_height) {
      best_under_limit = variant;
    }
  }

  if (best_under_limit.has_value()) {
    return best_under_limit;
  }

  return variants.front();
}

}  // namespace chzzk
