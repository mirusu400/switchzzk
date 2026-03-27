#pragma once

#include <optional>
#include <string>
#include <vector>

#include "chzzk/models.hpp"

namespace chzzk {

struct VariantStream {
  std::string uri;
  PlaybackResolution resolution;
  int bandwidth = 0;
};

std::vector<VariantStream> parse_variant_playlist(
    const std::string& master_playlist,
    const std::string& master_url);

std::optional<VariantStream> choose_variant(
    const std::vector<VariantStream>& variants,
    int max_height);

std::string resolve_relative_url(const std::string& base_url,
                                 const std::string& relative_or_absolute);

}  // namespace chzzk
