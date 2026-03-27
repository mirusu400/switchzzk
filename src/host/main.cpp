#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "chzzk/chzzk_client.hpp"

namespace {

void print_help() {
  std::cout
      << "Commands\n"
      << "  j: next live\n"
      << "  k: previous live\n"
      << "  d: show current live detail\n"
      << "  p: resolve playback URL\n"
      << "  l: toggle low-latency preference\n"
      << "  r: refresh live list\n"
      << "  q: quit\n\n";
}

void render_list(const chzzk::LiveListResponse& lives,
                 std::size_t selected_index,
                 bool low_latency) {
  std::cout << "\n=== CHZZK Switch MVP (host validation) ===\n";
  std::cout << "Selected: " << (selected_index + 1) << "/" << lives.data.size()
            << " | Low latency: " << (low_latency ? "ON" : "OFF") << "\n\n";

  for (std::size_t index = 0; index < lives.data.size(); ++index) {
    const auto& item = lives.data[index];
    std::cout << (index == selected_index ? "> " : "  ");
    std::cout << item.channel.channel_name << " | " << item.live_title
              << " | viewers=" << item.concurrent_user_count;
    if (!item.live_category_value.empty()) {
      std::cout << " | " << item.live_category_value;
    }
    std::cout << "\n";
  }

  std::cout << "\n";
  print_help();
  std::cout << "> " << std::flush;
}

std::optional<chzzk::LiveListResponse> fetch_lives(chzzk::ChzzkClient& client) {
  return client.get_popular_lives(10);
}

}  // namespace

int main(int argc, char* argv[]) {
  const bool network_mode = argc > 1 && std::string(argv[1]) == "--network";
  const auto fixture_root = std::filesystem::current_path() / "fixtures";

  std::unique_ptr<chzzk::HttpClient> http_client;
  if (network_mode) {
    http_client = std::make_unique<chzzk::HttpsHttpClient>();
  } else {
    http_client = std::make_unique<chzzk::FixtureHttpClient>(fixture_root.string());
  }

  auto client = std::make_unique<chzzk::ChzzkClient>(*http_client);
  auto lives = fetch_lives(*client);

  if (!lives.has_value() && network_mode) {
    std::cerr << "Network fetch failed. Falling back to fixtures.\n";
    http_client = std::make_unique<chzzk::FixtureHttpClient>(fixture_root.string());
    client = std::make_unique<chzzk::ChzzkClient>(*http_client);
    lives = fetch_lives(*client);
  }

  if (!lives.has_value() || lives->data.empty()) {
    std::cerr << "Unable to load live list.\n";
    return 1;
  }

  std::size_t selected_index = 0;
  bool low_latency = false;

  while (true) {
    render_list(*lives, selected_index, low_latency);

    std::string command;
    if (!std::getline(std::cin, command)) {
      break;
    }
    if (command.empty()) {
      continue;
    }

    switch (command[0]) {
      case 'j':
        selected_index = (selected_index + 1) % lives->data.size();
        break;
      case 'k':
        selected_index =
            (selected_index + lives->data.size() - 1) % lives->data.size();
        break;
      case 'l':
        low_latency = !low_latency;
        break;
      case 'r':
        if (auto refreshed = fetch_lives(*client); refreshed.has_value() &&
                                                !refreshed->data.empty()) {
          lives = std::move(refreshed);
          selected_index = 0;
        }
        break;
      case 'd': {
        const auto& selected = lives->data[selected_index];
        auto detail = client->get_live_detail(selected.channel.channel_id);
        if (!detail.has_value()) {
          std::cout << "Detail fetch failed.\n";
          break;
        }

        std::cout << "\n--- Detail ---\n";
        std::cout << "Channel: " << detail->channel.channel_name << "\n";
        std::cout << "Title: " << detail->live_title << "\n";
        std::cout << "Viewers: " << detail->concurrent_user_count << "\n";
        std::cout << "Category: " << detail->live_category_value << "\n";
        std::cout << "Media entries: " << detail->media.size() << "\n\n";
        break;
      }
      case 'p': {
        const auto& selected = lives->data[selected_index];
        auto detail = client->get_live_detail(selected.channel.channel_id);
        if (!detail.has_value()) {
          std::cout << "Detail fetch failed.\n";
          break;
        }

        chzzk::PlaybackPreference preference;
        preference.prefer_low_latency = low_latency;
        preference.max_height = 720;

        const auto playback = client->resolve_playback(*detail, preference);
        if (!playback.has_value()) {
          std::cout << "Playback resolution failed.\n";
          break;
        }

        std::cout << "\n--- Playback ---\n";
        std::cout << "Media: " << playback->media_id << "\n";
        std::cout << "Master: " << playback->master_url << "\n";
        std::cout << "Selected: " << playback->selected_url << "\n";
        if (playback->resolution.height > 0) {
          std::cout << "Resolution: " << playback->resolution.width << "x"
                    << playback->resolution.height << "\n";
        }
        std::cout << "\n";
        break;
      }
      case 'q':
        return 0;
      default:
        break;
    }
  }

  return 0;
}
