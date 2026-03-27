#pragma once

#include <string>

namespace chzzk {

struct SwitchPlaybackRequest {
  std::string title;
  std::string url;
  std::string referer;
  std::string http_header_fields;
};

bool run_switch_player(const SwitchPlaybackRequest& request, std::string& error);

}  // namespace chzzk
