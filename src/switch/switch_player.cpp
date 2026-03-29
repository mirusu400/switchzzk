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

namespace chzzk {
namespace {

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
    mpv_set_option_string(mpv_, "input-default-bindings", "no");
    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "keep-open", "no");
    mpv_set_option_string(mpv_, "ytdl", "no");
    mpv_set_option_string(mpv_, "config", "no");
    mpv_set_option_string(mpv_, "user-agent", "switch-chzzk/0.1");
    mpv_set_option_string(mpv_, "tls-verify", "no");
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

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
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
          if (event.jbutton.button == 1 || event.jbutton.button == 10) {
            running = false;
          } else if (event.jbutton.button == 0) {
            const char* cmd[] = {"cycle", "pause", nullptr};
            mpv_command(mpv_, cmd);
            paused = !paused;
            force_redraw = true;
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
          }
        }

        // Joy-Con HAT (D-pad) 시크
        if (event.type == SDL_JOYHATMOTION) {
          if (event.jhat.value & SDL_HAT_RIGHT) {
            const char* cmd[] = {"seek", "10", nullptr};
            mpv_command(mpv_, cmd);
          } else if (event.jhat.value & SDL_HAT_LEFT) {
            const char* cmd[] = {"seek", "-10", nullptr};
            mpv_command(mpv_, cmd);
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
};

}  // namespace

bool run_switch_player(const SwitchPlaybackRequest& request, std::string& error) {
  SwitchPlayer player(request);
  return player.run(error);
}

}  // namespace chzzk
