#include <optional>
#include <cstring>
#include <fstream>
#include <string>

#include <switch.h>

#include "chzzk/chzzk_client.hpp"
#include "chzzk/switch_player.hpp"

namespace {

void append_log(const std::string& message) {
  std::ofstream log("sdmc:/switch/switch_chzzk.log", std::ios::app);
  if (log.is_open()) {
    log << message << '\n';
  }
}

std::string origin_from_url(const std::string& url) {
  const std::size_t scheme_pos = url.find("://");
  if (scheme_pos == std::string::npos) {
    return "https://chzzk.naver.com/";
  }
  const std::size_t path_pos = url.find('/', scheme_pos + 3);
  if (path_pos == std::string::npos) {
    return url + "/";
  }
  return url.substr(0, path_pos + 1);
}

std::string first_line(const std::string& text) {
  const std::size_t newline = text.find('\n');
  if (newline == std::string::npos) {
    return text;
  }
  return text.substr(0, newline);
}

void print_live_list(const chzzk::LiveListResponse& lives,
                     std::size_t selected_index,
                     bool low_latency,
                     const std::string& status) {
  consoleClear();
  printf("CHZZK Switch MVP\n\n");
  printf("A: resolve stream  X: refresh  Y: toggle LL-HLS  B: exit\n");
  printf("Selected %zu/%zu | LL-HLS %s\n", selected_index + 1, lives.data.size(),
         low_latency ? "ON" : "OFF");
  if (!status.empty()) {
    printf("%s\n", status.c_str());
  }
  printf("\n");

  for (std::size_t i = 0; i < lives.data.size(); ++i) {
    const auto& item = lives.data[i];
    printf("%c %s | %s | %d viewers\n", i == selected_index ? '>' : ' ',
           item.channel.channel_name.c_str(), item.live_title.c_str(),
           item.concurrent_user_count);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  append_log("main: start");
  socketInitializeDefault();
  PrintConsole main_console{};
  consoleInit(&main_console);
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad;
  padInitializeDefault(&pad);

  chzzk::HttpsHttpClient http_client;
  chzzk::ChzzkClient client(http_client);

  std::optional<chzzk::LiveListResponse> lives = client.get_popular_lives(12);
  std::size_t selected_index = 0;
  bool low_latency = false;
  std::string status = "Fetching CHZZK live list...";

  if (!lives.has_value() || lives->data.empty()) {
    status =
        "Live list fetch failed. Check network and CHZZK API accessibility.";
  } else {
    status = "Press A to resolve the playback URL for the selected live.";
  }

  while (appletMainLoop()) {
    append_log("main: loop begin");
    padUpdate(&pad);
    const u64 keys_down = padGetButtonsDown(&pad);

    if (keys_down & HidNpadButton_B) {
      break;
    }

    if (lives.has_value() && !lives->data.empty()) {
      if (keys_down & HidNpadButton_Down) {
        selected_index = (selected_index + 1) % lives->data.size();
      }
      if (keys_down & HidNpadButton_Up) {
        selected_index =
            (selected_index + lives->data.size() - 1) % lives->data.size();
      }
      if (keys_down & HidNpadButton_Y) {
        low_latency = !low_latency;
        status = low_latency ? "Low-latency playback requested."
                             : "Standard HLS playback requested.";
      }
      if (keys_down & HidNpadButton_X) {
        lives = client.get_popular_lives(12);
        if (!lives.has_value() || lives->data.empty()) {
          status = "Refresh failed.";
        } else {
          selected_index = 0;
          status = "Live list refreshed.";
        }
      }
      if (keys_down & HidNpadButton_A) {
        const auto& selected = lives->data[selected_index];
        append_log("main: A pressed for channel " + selected.channel.channel_id);
        const auto detail = client.get_live_detail(selected.channel.channel_id);
        if (!detail.has_value()) {
          status = "Live detail fetch failed.";
          append_log("main: detail fetch failed");
        } else {
          chzzk::PlaybackPreference preference;
          preference.prefer_low_latency = low_latency;
          preference.max_height = 720;

          const auto playback = client.resolve_playback(*detail, preference);
          if (!playback.has_value()) {
            status = "Playback URL resolution failed.";
            append_log("main: playback resolution failed");
          } else {
            append_log("main: preflight fetch begin");
            const auto preflight = http_client.get(
                playback->selected_url,
                {
                    {"User-Agent", "switch-chzzk/0.1"},
                    {"Accept", "*/*"},
                });
            if (!preflight.has_value()) {
              status = "Playback preflight fetch failed.";
              append_log("main: preflight fetch failed");
              continue;
            }

            append_log("main: preflight fetch ok bytes=" +
                       std::to_string(preflight->size()) +
                       " first_line=" + first_line(*preflight));

            append_log("main: launching player");
            consoleClear();
            printf("Launching player...\n");
            consoleUpdate(nullptr);
            append_log("main: consoleExit before player");
            consoleExit(&main_console);

            std::string player_error;
            const bool ok = chzzk::run_switch_player(
                chzzk::SwitchPlaybackRequest{
                    .title = detail->live_title,
                    .url = playback->selected_url,
                    .referer = origin_from_url(playback->selected_url),
                    .http_header_fields =
                        "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache",
                },
                player_error);

            append_log("main: console reinit begin");
            std::memset(&main_console, 0, sizeof(main_console));
            consoleInit(&main_console);
            consoleSelect(&main_console);
            padConfigureInput(1, HidNpadStyleSet_NpadStandard);
            padInitializeDefault(&pad);
            append_log("main: console reinit done");

            append_log("main: player returned ok=" + std::string(ok ? "true" : "false") +
                       " error=" + player_error);

            status = ok ? "Playback ended. Returned to live list."
                        : "Playback failed: " + player_error;
          }
        }
      }
    }

    if (lives.has_value() && !lives->data.empty()) {
      append_log("main: print live list");
      print_live_list(*lives, selected_index, low_latency, status);
    } else {
      append_log("main: print empty state");
      consoleClear();
      printf("CHZZK Switch MVP\n\n%s\n", status.c_str());
      printf("\nB: exit  X: retry\n");
      if (keys_down & HidNpadButton_X) {
        lives = client.get_popular_lives(12);
        if (!lives.has_value() || lives->data.empty()) {
          status = "Retry failed.";
        } else {
          selected_index = 0;
          status = "Live list refreshed.";
        }
      }
    }

    append_log("main: consoleUpdate");
    consoleUpdate(nullptr);
  }

  append_log("main: exit");
  consoleExit(&main_console);
  socketExit();
  return 0;
}
