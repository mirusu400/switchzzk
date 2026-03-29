#pragma once

#include <borealis.hpp>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>

namespace chzzk {

class ImageLoader {
  public:
    static ImageLoader& instance();

    void load(const std::string& url, brls::Image* target);

    void start();
    void stop();

  private:
    ImageLoader() = default;
    ~ImageLoader();

    void worker();

    struct AsyncRequest {
      std::string url;
      std::function<void(const std::string&, size_t)> callback;
    };

    std::thread thread_;
    std::mutex mutex_;
    std::queue<AsyncRequest> queue_;
    std::atomic<bool> running_{false};
};

}  // namespace chzzk
