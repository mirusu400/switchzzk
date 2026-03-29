#include "chzzk/image_loader.hpp"
#include "chzzk/http_client.hpp"
#include <cstdio>

extern FILE* g_logfile;
extern void dbg(const char* msg);

namespace chzzk {

ImageLoader& ImageLoader::instance() {
    static ImageLoader inst;
    return inst;
}

ImageLoader::~ImageLoader() {
    stop();
}

void ImageLoader::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread([this]() { worker(); });
}

void ImageLoader::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void ImageLoader::load(const std::string& url, brls::Image* target) {
    // Borealis의 setImageAsync를 사용 — 안전한 라이프사이클 관리
    target->setImageAsync([url, this](std::function<void(const std::string&, size_t)> cb) {
        // 워커 스레드 큐에 넣기
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({url, cb});
    });
}

void ImageLoader::worker() {
    dbg("ImageLoader: worker started");
    HttpsHttpClient client;

    while (running_) {
        AsyncRequest req;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!queue_.empty()) {
                req = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (req.url.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        auto data = client.get(req.url);
        if (data.has_value() && !data->empty()) {
            // setImageAsync의 콜백 호출 → 내부에서 brls::sync로 안전하게 처리
            req.callback(*data, data->size());
        } else {
            req.callback("", 0);
        }
    }
    dbg("ImageLoader: worker exited");
}

}  // namespace chzzk
