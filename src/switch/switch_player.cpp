#include "chzzk/switch_player.hpp"

#include <atomic>
#include <clocale>
#include <fstream>
#include <string>

#include <switch.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <mpv/stream_cb.h>

#ifdef __SWITCH__
#include <curl/curl.h>
#endif

// 전역 (namespace 밖) — vod_tab.cpp에서 extern으로 접근

namespace chzzk {
namespace {

// ─── curl→mpv stream bridge ───
// mpv가 URL을 열 때 ffmpeg 대신 libcurl로 HTTP를 처리

struct CurlStream {
  CURL* curl = nullptr;
  std::string url;
  std::string data;
  size_t pos = 0;
  bool fetched = false;
};

static size_t curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userp) {
  auto* s = static_cast<CurlStream*>(userp);
  s->data.append(static_cast<const char*>(ptr), size * nmemb);
  return size * nmemb;
}

static void curl_stream_fetch(CurlStream* s) {
  if (s->fetched) return;
  s->fetched = true;
  s->curl = curl_easy_init();
  if (!s->curl) return;
  curl_easy_setopt(s->curl, CURLOPT_URL, s->url.c_str());
  curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, s);
  curl_easy_setopt(s->curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(s->curl, CURLOPT_USERAGENT,
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36");
  curl_easy_setopt(s->curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(s->curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_perform(s->curl);
  curl_easy_cleanup(s->curl);
  s->curl = nullptr;
}

static int64_t curl_stream_read(void* cookie, char* buf, uint64_t nbytes) {
  auto* s = static_cast<CurlStream*>(cookie);
  curl_stream_fetch(s);
  if (s->pos >= s->data.size()) return 0;
  size_t avail = s->data.size() - s->pos;
  size_t to_read = (nbytes < avail) ? nbytes : avail;
  memcpy(buf, s->data.data() + s->pos, to_read);
  s->pos += to_read;
  return static_cast<int64_t>(to_read);
}

static int64_t curl_stream_seek(void* cookie, int64_t offset) {
  auto* s = static_cast<CurlStream*>(cookie);
  curl_stream_fetch(s);
  if (offset < 0 || static_cast<size_t>(offset) > s->data.size())
    return MPV_ERROR_GENERIC;
  s->pos = static_cast<size_t>(offset);
  return static_cast<int64_t>(s->pos);
}

static int64_t curl_stream_size(void* cookie) {
  auto* s = static_cast<CurlStream*>(cookie);
  curl_stream_fetch(s);
  return static_cast<int64_t>(s->data.size());
}

static void curl_stream_close(void* cookie) {
  auto* s = static_cast<CurlStream*>(cookie);
  delete s;
}

static int curl_stream_open(void* user_data, char* uri, mpv_stream_cb_info* info) {
  // uri = "chzzkcurl://https://actual.url/path"
  std::string url_str(uri);
  const std::string prefix = "chzzkcurl://";
  if (url_str.find(prefix) != 0) return MPV_ERROR_LOADING_FAILED;
  std::string real_url = url_str.substr(prefix.size());

  auto* s = new CurlStream();
  s->url = real_url;

  info->cookie = s;
  info->read_fn = curl_stream_read;
  info->seek_fn = curl_stream_seek;
  info->size_fn = curl_stream_size;
  info->close_fn = curl_stream_close;
  return 0;
}

// g_vod_segments, g_vod_ready는 파일 상단에서 전역 선언됨

struct VodStream {
  size_t seg_idx = 0;      // 현재 세그먼트 인덱스
  std::string buf;          // 현재 세그먼트 데이터
  size_t buf_pos = 0;       // 버퍼 내 읽기 위치
  bool done = false;
};

static size_t vod_curl_write(void* ptr, size_t size, size_t nmemb, void* userp) {
  auto* buf = static_cast<std::string*>(userp);
  buf->append(static_cast<const char*>(ptr), size * nmemb);
  return size * nmemb;
}

static void vod_fetch_next(VodStream* s) {
  if (s->seg_idx >= g_vod_segments.size()) {
    s->done = true;
    return;
  }
  s->buf.clear();
  s->buf_pos = 0;

  CURL* curl = curl_easy_init();
  if (!curl) { s->done = true; return; }
  curl_easy_setopt(curl, CURLOPT_URL, g_vod_segments[s->seg_idx].c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, vod_curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s->buf);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) { s->done = true; return; }
  s->seg_idx++;
}

static int64_t vod_stream_read(void* cookie, char* buf, uint64_t nbytes) {
  auto* s = static_cast<VodStream*>(cookie);
  if (s->done) return 0;

  // 현재 버퍼가 비었으면 다음 세그먼트 fetch
  while (s->buf_pos >= s->buf.size()) {
    if (s->done) return 0;
    vod_fetch_next(s);
    if (s->done && s->buf.empty()) return 0;
  }

  size_t avail = s->buf.size() - s->buf_pos;
  size_t to_read = (nbytes < avail) ? static_cast<size_t>(nbytes) : avail;
  memcpy(buf, s->buf.data() + s->buf_pos, to_read);
  s->buf_pos += to_read;
  return static_cast<int64_t>(to_read);
}

static void vod_stream_close(void* cookie) {
  delete static_cast<VodStream*>(cookie);
}

static int vod_stream_open(void* user_data, char* uri, mpv_stream_cb_info* info) {
  if (!g_vod_ready) return MPV_ERROR_LOADING_FAILED;

  auto* s = new VodStream();
  // 첫 세그먼트 미리 로드
  vod_fetch_next(s);

  info->cookie = s;
  info->read_fn = vod_stream_read;
  info->seek_fn = nullptr;  // VOD 시크 불가 (순차 스트림)
  info->size_fn = nullptr;
  info->close_fn = vod_stream_close;
  return 0;
}

void append_log(const std::string& message) {
  std::ofstream log("sdmc:/switch/switch_chzzk.log", std::ios::app);
  if (log.is_open()) {
    log << message << '\n';
  }
}

const char* end_reason_to_string(mpv_end_file_reason reason) {
  switch (reason) {
    case MPV_END_FILE_REASON_EOF:
      return "eof";
    case MPV_END_FILE_REASON_STOP:
      return "stop";
    case MPV_END_FILE_REASON_QUIT:
      return "quit";
    case MPV_END_FILE_REASON_ERROR:
      return "error";
    case MPV_END_FILE_REASON_REDIRECT:
      return "redirect";
    default:
      return "unknown";
  }
}

class SwitchPlayer {
public:
  explicit SwitchPlayer(const SwitchPlaybackRequest& request) : request_(request) {}

  ~SwitchPlayer() { cleanup(); }

  bool run(std::string& error) {
    append_log("player: run() begin");
    if (!init_sdl(error)) {
      append_log("player: init_sdl failed: " + error);
      return false;
    }
    if (!init_mpv(error)) {
      append_log("player: init_mpv failed: " + error);
      return false;
    }
    if (!load_file(error)) {
      append_log("player: load_file failed: " + error);
      return false;
    }

    // loadfile 직후 mpv 이벤트를 강제 수집 (빠른 실패 시 로그 누락 방지)
    append_log("player: waiting for mpv events after loadfile");
    for (int i = 0; i < 100; ++i) {
      mpv_event* ev = mpv_wait_event(mpv_, 0.1);  // 100ms 대기
      if (!ev || ev->event_id == MPV_EVENT_NONE) continue;
      if (ev->event_id == MPV_EVENT_LOG_MESSAGE) {
        const auto* msg = static_cast<mpv_event_log_message*>(ev->data);
        if (msg && msg->text) {
          std::string text = msg->text;
          while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();
          append_log(std::string("player: mpv early [") + (msg->level ? msg->level : "?") +
                     "] " + (msg->prefix ? msg->prefix : "?") + ": " + text);
        }
      } else if (ev->event_id == MPV_EVENT_START_FILE) {
        append_log("player: mpv start-file event (early)");
      } else if (ev->event_id == MPV_EVENT_END_FILE) {
        const auto* end = static_cast<mpv_event_end_file*>(ev->data);
        append_log("player: mpv end-file event (early) reason=" +
                   std::string(end_reason_to_string(end->reason)) +
                   " error=" + std::to_string(end->error) +
                   " error_text=" + std::string(mpv_error_string(end->error)));
        // 에러지만 계속 수집해서 앞에 쌓인 로그 메시지를 모두 남김
      }
    }
    append_log("player: early drain done, entering loop");
    loop();
    append_log("player: loop ended");
    error = terminal_error_;
    return terminal_error_.empty();
  }

private:
  static void* get_proc_address(void* /*ctx*/, const char* name) {
    return SDL_GL_GetProcAddress(name);
  }

  static void on_mpv_wakeup(void* ctx) {
    static_cast<SwitchPlayer*>(ctx)->mpv_events_pending_.store(true);
  }

  static void on_render_update(void* ctx) {
    static_cast<SwitchPlayer*>(ctx)->render_update_pending_.store(true);
  }

  bool init_sdl(std::string& error) {
    append_log("player: SDL_Init");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
      error = std::string("SDL_Init failed: ") + SDL_GetError();
      return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    const AppletOperationMode mode = appletGetOperationMode();
    int width = mode == AppletOperationMode_Console ? 1920 : 1280;
    int height = mode == AppletOperationMode_Console ? 1080 : 720;
    append_log("player: operation mode=" +
               std::string(mode == AppletOperationMode_Console ? "console" : "handheld"));

    append_log("player: SDL_CreateWindow");
    window_ = SDL_CreateWindow(
        "switch-chzzk-player",
        0,
        0,
        width,
        height,
        SDL_WINDOW_SHOWN);
    if (!window_ && (width != 1280 || height != 720)) {
      append_log("player: SDL_CreateWindow primary failed, retrying 1280x720: " +
                 std::string(SDL_GetError()));
      window_ = SDL_CreateWindow(
          "switch-chzzk-player",
          0,
          0,
          1280,
          720,
          SDL_WINDOW_SHOWN);
    }
    if (!window_) {
      error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
      return false;
    }

    append_log("player: SDL_GL_CreateContext");
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
      error = std::string("SDL_GL_CreateContext failed: ") + SDL_GetError();
      return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);

    append_log("player: joystick scan");
    for (int i = 0; i < SDL_NumJoysticks() && i < 2; ++i) {
      SDL_JoystickOpen(i);
    }

    return true;
  }

  bool init_mpv(std::string& error) {
    setlocale(LC_NUMERIC, "C");
    append_log("player: mpv_create");
    mpv_ = mpv_create();
    if (!mpv_) {
      error = "mpv_create failed";
      return false;
    }

    mpv_set_option_string(mpv_, "vo", "libmpv");
    mpv_set_option_string(mpv_, "hwdec", "auto-safe");
    mpv_set_option_string(mpv_, "profile", "sw-fast");
    mpv_set_option_string(mpv_, "osc", "no");
    mpv_set_option_string(mpv_, "osd-level", "1");
    mpv_set_option_string(mpv_, "osd-font-size", "36");
    mpv_set_option_string(mpv_, "osd-duration", "1500");
    mpv_set_option_string(mpv_, "input-default-bindings", "no");
    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "keep-open", "yes");
    mpv_set_option_string(mpv_, "ytdl", "no");
    mpv_set_option_string(mpv_, "force-seekable", "yes");
    mpv_set_option_string(mpv_, "config", "no");
    mpv_set_option_string(mpv_, "user-agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36");
    mpv_set_option_string(mpv_, "tls-verify", "no");
    mpv_set_option_string(mpv_, "demuxer-lavf-o",
        "protocol_whitelist=file,http,https,tcp,tls,crypto,data,chzzkcurl");
    mpv_set_option_string(mpv_, "msg-level", "all=v");
    if (!request_.referer.empty()) {
      mpv_set_option_string(mpv_, "referrer", request_.referer.c_str());
      append_log("player: mpv referrer=" + request_.referer);
    }
    if (!request_.http_header_fields.empty()) {
      mpv_set_option_string(mpv_, "http-header-fields",
                            request_.http_header_fields.c_str());
      append_log("player: mpv http-header-fields=" + request_.http_header_fields);
    }

    // 커스텀 프로토콜 등록
    mpv_stream_cb_add_ro(mpv_, "chzzkcurl", nullptr, curl_stream_open);
    mpv_stream_cb_add_ro(mpv_, "chzzkvod", nullptr, vod_stream_open);

    append_log("player: mpv_initialize");
    if (mpv_initialize(mpv_) < 0) {
      error = "mpv_initialize failed";
      return false;
    }

    mpv_request_log_messages(mpv_, "debug");
    mpv_set_wakeup_callback(mpv_, on_mpv_wakeup, this);

    mpv_opengl_init_params gl_init{
        .get_proc_address = get_proc_address,
        .get_proc_address_ctx = nullptr,
    };

    int advanced_control = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced_control},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    append_log("player: mpv_render_context_create");
    if (mpv_render_context_create(&render_context_, mpv_, params) < 0) {
      error = "mpv_render_context_create failed";
      return false;
    }

    mpv_render_context_set_update_callback(render_context_, on_render_update, this);
    return true;
  }

  bool load_file(std::string& error) {
    append_log("player: loadfile " + request_.url);
    const char* cmd[] = {"loadfile", request_.url.c_str(), nullptr};
    if (mpv_command(mpv_, cmd) < 0) {
      error = "mpv loadfile failed";
      return false;
    }
    return true;
  }

  void loop() {
    bool running = true;
    bool paused = false;
    bool force_redraw = true;

    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          running = false;
        }

        if (event.type == SDL_JOYBUTTONDOWN) {
          int btn = event.jbutton.button;
          append_log("player: joybtn=" + std::to_string(btn));
          if (btn == 1 || btn == 10) {
            running = false;
          } else if (btn == 0) {
            const char* cmd[] = {"cycle", "pause", nullptr};
            mpv_command(mpv_, cmd);
            paused = !paused;
            force_redraw = true;
          } else if (btn == 13) {  // D-pad Up
            const char* cmd[] = {"add", "volume", "5", nullptr};
            mpv_command(mpv_, cmd);
            {
              double vol = 0;
              mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);
              volume_display_ = static_cast<int>(vol);
              volume_display_timer_ = 90; // ~1.5초 (60fps)
            }
          } else if (btn == 15) {  // D-pad Down
            const char* cmd[] = {"add", "volume", "-5", nullptr};
            mpv_command(mpv_, cmd);
            {
              double vol = 0;
              mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);
              volume_display_ = static_cast<int>(vol);
              volume_display_timer_ = 90; // ~1.5초 (60fps)
            }
          } else if (btn == 14) {  // D-pad Right
            const char* cmd[] = {"seek", "10", nullptr};
            mpv_command(mpv_, cmd);
          } else if (btn == 12) {  // D-pad Left
            const char* cmd[] = {"seek", "-10", nullptr};
            mpv_command(mpv_, cmd);
          }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
          if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            running = false;
          } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            const char* cmd[] = {"cycle", "pause", nullptr};
            mpv_command(mpv_, cmd);
            paused = !paused;
            force_redraw = true;
          } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            const char* cmd[] = {"seek", "10", nullptr};
            mpv_command(mpv_, cmd);
          } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            const char* cmd[] = {"seek", "-10", nullptr};
            mpv_command(mpv_, cmd);
          } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
            const char* cmd[] = {"add", "volume", "5", nullptr};
            mpv_command(mpv_, cmd);
            {
              double vol = 0;
              mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);
              volume_display_ = static_cast<int>(vol);
              volume_display_timer_ = 90; // ~1.5초 (60fps)
            }
          } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
            const char* cmd[] = {"add", "volume", "-5", nullptr};
            mpv_command(mpv_, cmd);
            {
              double vol = 0;
              mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);
              volume_display_ = static_cast<int>(vol);
              volume_display_timer_ = 90; // ~1.5초 (60fps)
            }
          }
        }

        // Joy-Con HAT (D-pad)
        if (event.type == SDL_JOYHATMOTION) {
          if (event.jhat.value & SDL_HAT_RIGHT) {
            const char* cmd[] = {"seek", "10", nullptr};
            mpv_command(mpv_, cmd);
          } else if (event.jhat.value & SDL_HAT_LEFT) {
            const char* cmd[] = {"seek", "-10", nullptr};
            mpv_command(mpv_, cmd);
          } else if (event.jhat.value & SDL_HAT_UP) {
            const char* cmd[] = {"add", "volume", "5", nullptr};
            mpv_command(mpv_, cmd);
            {
              double vol = 0;
              mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);
              volume_display_ = static_cast<int>(vol);
              volume_display_timer_ = 90; // ~1.5초 (60fps)
            }
          } else if (event.jhat.value & SDL_HAT_DOWN) {
            const char* cmd[] = {"add", "volume", "-5", nullptr};
            mpv_command(mpv_, cmd);
            {
              double vol = 0;
              mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);
              volume_display_ = static_cast<int>(vol);
              volume_display_timer_ = 90; // ~1.5초 (60fps)
            }
          }
        }
      }

      if (mpv_events_pending_.exchange(false)) {
        if (!drain_mpv_events()) {
          running = false;
        }
      }

      bool should_render = force_redraw;
      if (render_update_pending_.exchange(false)) {
        should_render = (mpv_render_context_update(render_context_) &
                         MPV_RENDER_UPDATE_FRAME) != 0;
      }

      if (should_render) {
        render_frame();
        force_redraw = paused;
      } else {
        SDL_Delay(10);
      }
    }
  }

  bool drain_mpv_events() {
    while (true) {
      mpv_event* event = mpv_wait_event(mpv_, 0);
      if (!event || event->event_id == MPV_EVENT_NONE) {
        return true;
      }

      if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
        const auto* message =
            static_cast<mpv_event_log_message*>(event->data);
        if (message && message->prefix && message->level && message->text) {
          std::string text = message->text;
          while (!text.empty() &&
                 (text.back() == '\n' || text.back() == '\r')) {
            text.pop_back();
          }
          append_log("player: mpv log [" + std::string(message->level) + "] " +
                     std::string(message->prefix) + ": " + text);
        }
        continue;
      }

      if (event->event_id == MPV_EVENT_START_FILE) {
        append_log("player: mpv start-file event");
        continue;
      }

      if (event->event_id == MPV_EVENT_FILE_LOADED) {
        append_log("player: mpv file-loaded event");
        continue;
      }

      if (event->event_id == MPV_EVENT_VIDEO_RECONFIG) {
        append_log("player: mpv video-reconfig event");
        continue;
      }

      if (event->event_id == MPV_EVENT_AUDIO_RECONFIG) {
        append_log("player: mpv audio-reconfig event");
        continue;
      }

      if (event->event_id == MPV_EVENT_END_FILE) {
        const auto* end_file = static_cast<mpv_event_end_file*>(event->data);
        std::string message = "player: mpv end-file event";
        if (end_file) {
          message += " reason=" + std::string(end_reason_to_string(end_file->reason));
          message += " error=" + std::to_string(end_file->error);
          if (end_file->error != 0) {
            message += " error_text=" + std::string(mpv_error_string(end_file->error));
          }
        }
        append_log(message);
        if (end_file && end_file->reason == MPV_END_FILE_REASON_REDIRECT) {
          continue;
        }
        // EOF — VOD 끝 또는 라이브 종료
        if (end_file && end_file->reason == MPV_END_FILE_REASON_EOF) {
          return false;
        }
        if (end_file && end_file->reason == MPV_END_FILE_REASON_ERROR) {
          terminal_error_ = end_file->error != 0
                                ? "mpv load failed: " + std::string(mpv_error_string(end_file->error))
                                : "mpv load failed";
        }
        return false;
      }

      if (event->event_id == MPV_EVENT_SHUTDOWN) {
        append_log("player: mpv shutdown event");
        if (terminal_error_.empty()) {
          terminal_error_ = "mpv shutdown";
        }
        return false;
      }
    }
  }

  void render_frame() {
    int width = 1280;
    int height = 720;
    SDL_GL_GetDrawableSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mpv_opengl_fbo fbo{
        .fbo = 0,
        .w = width,
        .h = height,
        .internal_format = 0,
    };
    int flip_y = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    mpv_render_context_render(render_context_, params);

    // 볼륨 바 오버레이 (GL ES 2.0 — glScissor로 사각형)
    if (volume_display_timer_ > 0) {
      volume_display_timer_--;
      float vol = volume_display_ / 100.0f;
      if (vol > 1.0f) vol = 1.0f;
      if (vol < 0.0f) vol = 0.0f;

      int bar_total_w = width * 60 / 100;
      int bar_h_px = height * 3 / 100;
      int bar_x = (width - bar_total_w) / 2;
      int bar_y = height * 8 / 100;  // 하단에서 8%

      // 배경 바 (어두운 회색)
      glEnable(GL_SCISSOR_TEST);
      glScissor(bar_x, bar_y, bar_total_w, bar_h_px);
      glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      // 채워진 부분 (치지직 그린 #00FFA3)
      int fill_w = static_cast<int>(bar_total_w * vol);
      if (fill_w > 0) {
        glScissor(bar_x, bar_y, fill_w, bar_h_px);
        glClearColor(0.0f, 1.0f, 0.64f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
      }

      glDisable(GL_SCISSOR_TEST);
    }

    SDL_GL_SwapWindow(window_);
    mpv_render_context_report_swap(render_context_);
  }

  void cleanup() {
    append_log("player: cleanup begin");
    if (render_context_) {
      append_log("player: cleanup render_context");
      mpv_render_context_set_update_callback(render_context_, nullptr, nullptr);
      mpv_render_context_free(render_context_);
      render_context_ = nullptr;
    }

    if (mpv_) {
      append_log("player: cleanup mpv");
      mpv_set_wakeup_callback(mpv_, nullptr, nullptr);
      mpv_destroy(mpv_);
      mpv_ = nullptr;
    }

    if (gl_context_) {
      append_log("player: cleanup gl_context");
      SDL_GL_MakeCurrent(window_, nullptr);
      SDL_GL_DeleteContext(gl_context_);
      gl_context_ = nullptr;
    }

    if (window_) {
      append_log("player: cleanup window");
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }

    append_log("player: cleanup SDL_Quit");
    SDL_Quit();
    append_log("player: cleanup done");
  }

  SwitchPlaybackRequest request_;
  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_context_ = nullptr;
  mpv_handle* mpv_ = nullptr;
  mpv_render_context* render_context_ = nullptr;
  std::atomic<bool> mpv_events_pending_{false};
  std::atomic<bool> render_update_pending_{false};
  std::string terminal_error_;
  int volume_display_ = -1;
  int volume_display_timer_ = 0;
};

}  // namespace

bool run_switch_player(const SwitchPlaybackRequest& request, std::string& error) {
  SwitchPlayer player(request);
  return player.run(error);
}

}  // namespace chzzk
