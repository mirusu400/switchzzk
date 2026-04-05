#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace chzzk {

struct ChatMessage {
    std::string nickname;
    std::string message;
    std::string badge_url;
    bool is_donation = false;
    int donation_amount = 0;
};

using ChatCallback = std::function<void(const ChatMessage&)>;

class ChatClient {
  public:
    ChatClient();
    ~ChatClient();

    // chatChannelId는 LiveDetail에서 가져옴
    bool connect(const std::string& chat_channel_id);
    void disconnect();
    bool isConnected() const { return connected_; }

    // 최근 메시지 가져오기
    std::vector<ChatMessage> getMessages();

    // 새 메시지 콜백
    void setCallback(ChatCallback cb) { callback_ = cb; }

  private:
    void recvLoop();
    bool doTlsConnect(const std::string& host, int port);
    bool doWebSocketHandshake(const std::string& host, const std::string& path);
    void sendWsFrame(const std::string& data);
    std::string recvWsFrame();
    void sendJson(const std::string& json);
    void handleMessage(const std::string& raw);

    int sock_ = -1;
    void* ssl_ctx_ = nullptr;  // mbedtls_ssl_context*
    void* ssl_conf_ = nullptr;
    void* net_ctx_ = nullptr;
    void* entropy_ = nullptr;
    void* ctr_drbg_ = nullptr;

    std::string chat_channel_id_;
    std::string sid_;
    std::atomic<bool> connected_{false};
    std::thread recv_thread_;

    std::mutex msg_mutex_;
    std::vector<ChatMessage> messages_;
    ChatCallback callback_;
};

}  // namespace chzzk
