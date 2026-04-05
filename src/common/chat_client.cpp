#include "chzzk/chat_client.hpp"
#include "nlohmann/json.hpp"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>

#ifdef __SWITCH__
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#else
// Host stub — WebSocket은 Switch 전용
#endif

extern FILE* g_logfile;
extern void dbg(const char* msg);

namespace chzzk {

using nlohmann::json;

ChatClient::ChatClient() {}

ChatClient::~ChatClient() {
    disconnect();
}

#ifdef __SWITCH__

bool ChatClient::doTlsConnect(const std::string& host, int port) {
    auto* entropy = new mbedtls_entropy_context;
    auto* ctr_drbg = new mbedtls_ctr_drbg_context;
    auto* ssl = new mbedtls_ssl_context;
    auto* conf = new mbedtls_ssl_config;
    auto* net = new mbedtls_net_context;

    mbedtls_net_init(net);
    mbedtls_ssl_init(ssl);
    mbedtls_ssl_config_init(conf);
    mbedtls_ctr_drbg_init(ctr_drbg);
    mbedtls_entropy_init(entropy);

    int ret = mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func, entropy, nullptr, 0);
    if (ret != 0) { dbg("chat: ctr_drbg_seed failed"); return false; }

    std::string port_str = std::to_string(port);
    ret = mbedtls_net_connect(net, host.c_str(), port_str.c_str(), MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        if (g_logfile) { fprintf(g_logfile, "chat: net_connect failed %d\n", ret); fflush(g_logfile); }
        return false;
    }

    ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) { dbg("chat: ssl_config_defaults failed"); return false; }

    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, ctr_drbg);

    ret = mbedtls_ssl_setup(ssl, conf);
    if (ret != 0) { dbg("chat: ssl_setup failed"); return false; }

    ret = mbedtls_ssl_set_hostname(ssl, host.c_str());
    if (ret != 0) { dbg("chat: ssl_set_hostname failed"); return false; }

    mbedtls_ssl_set_bio(ssl, net, mbedtls_net_send, mbedtls_net_recv, nullptr);

    do { ret = mbedtls_ssl_handshake(ssl); }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0) {
        if (g_logfile) { fprintf(g_logfile, "chat: ssl_handshake failed %d\n", ret); fflush(g_logfile); }
        return false;
    }

    ssl_ctx_ = ssl;
    ssl_conf_ = conf;
    net_ctx_ = net;
    entropy_ = entropy;
    ctr_drbg_ = ctr_drbg;
    dbg("chat: TLS connected");
    return true;
}

bool ChatClient::doWebSocketHandshake(const std::string& host, const std::string& path) {
    // Generate random key
    std::string key = "dGhlIHNhbXBsZSBub25jZQ==";  // base64 of "the sample nonce"

    std::string req = "GET " + path + " HTTP/1.1\r\n"
                      "Host: " + host + "\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: " + key + "\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Origin: https://chzzk.naver.com\r\n"
                      "\r\n";

    auto* ssl = static_cast<mbedtls_ssl_context*>(ssl_ctx_);
    int ret = mbedtls_ssl_write(ssl, reinterpret_cast<const unsigned char*>(req.c_str()), req.size());
    if (ret <= 0) { dbg("chat: ws handshake write failed"); return false; }

    // Read response
    char buf[2048];
    ret = mbedtls_ssl_read(ssl, reinterpret_cast<unsigned char*>(buf), sizeof(buf) - 1);
    if (ret <= 0) { dbg("chat: ws handshake read failed"); return false; }
    buf[ret] = '\0';

    if (strstr(buf, "101") == nullptr) {
        if (g_logfile) { fprintf(g_logfile, "chat: ws handshake failed: %s\n", buf); fflush(g_logfile); }
        return false;
    }

    dbg("chat: WebSocket handshake ok");
    return true;
}

void ChatClient::sendWsFrame(const std::string& data) {
    auto* ssl = static_cast<mbedtls_ssl_context*>(ssl_ctx_);
    std::vector<unsigned char> frame;

    // Text frame, FIN=1
    frame.push_back(0x81);

    // Length + mask bit
    size_t len = data.size();
    if (len < 126) {
        frame.push_back(static_cast<unsigned char>(len | 0x80));
    } else if (len < 65536) {
        frame.push_back(126 | 0x80);
        frame.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(len & 0xFF));
    } else {
        frame.push_back(127 | 0x80);
        for (int i = 7; i >= 0; i--)
            frame.push_back(static_cast<unsigned char>((len >> (8 * i)) & 0xFF));
    }

    // Masking key (required for client→server)
    unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame.insert(frame.end(), mask, mask + 4);

    // Masked data
    for (size_t i = 0; i < data.size(); i++)
        frame.push_back(static_cast<unsigned char>(data[i]) ^ mask[i % 4]);

    mbedtls_ssl_write(ssl, frame.data(), frame.size());
}

std::string ChatClient::recvWsFrame() {
    auto* ssl = static_cast<mbedtls_ssl_context*>(ssl_ctx_);
    unsigned char header[2];

    int ret = mbedtls_ssl_read(ssl, header, 2);
    if (ret <= 0) return "";

    unsigned char opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    size_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        unsigned char ext[2];
        mbedtls_ssl_read(ssl, ext, 2);
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        mbedtls_ssl_read(ssl, ext, 8);
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | ext[i];
    }

    unsigned char mask[4] = {};
    if (masked)
        mbedtls_ssl_read(ssl, mask, 4);

    std::string data(payload_len, '\0');
    size_t total = 0;
    while (total < payload_len) {
        ret = mbedtls_ssl_read(ssl, reinterpret_cast<unsigned char*>(&data[total]),
                                payload_len - total);
        if (ret <= 0) break;
        total += ret;
    }

    if (masked) {
        for (size_t i = 0; i < data.size(); i++)
            data[i] ^= mask[i % 4];
    }

    // Handle ping → pong
    if (opcode == 0x9) {
        // Send pong
        sendJson("{\"cmd\":10000,\"ver\":\"3\"}");
        return "";
    }

    // Close frame
    if (opcode == 0x8) {
        connected_ = false;
        return "";
    }

    return data;
}

void ChatClient::sendJson(const std::string& j) {
    sendWsFrame(j);
}

void ChatClient::handleMessage(const std::string& raw) {
    if (raw.empty()) return;

    auto root = json::parse(raw, nullptr, false);
    if (root.is_discarded()) return;

    int cmd = root.value("cmd", 0);

    // Connected response → get SID, request recent chat
    if (cmd == 10100) {
        if (root.contains("bdy") && root["bdy"].contains("sid"))
            sid_ = root["bdy"]["sid"].get<std::string>();
        dbg("chat: connected, requesting recent chat");

        json req;
        req["bdy"]["recentMessageCount"] = 50;
        req["cid"] = chat_channel_id_;
        req["cmd"] = 5101;
        req["sid"] = sid_;
        req["svcid"] = "game";
        req["tid"] = 2;
        req["ver"] = "3";
        sendJson(req.dump());
        return;
    }

    // Recent chat or live chat (cmd 15101 or 93101)
    if (cmd == 15101 || cmd == 93101) {
        auto& bdy_arr = root.contains("bdy") ? root["bdy"] : root;
        if (!bdy_arr.is_array()) {
            // bdy might be a single object with messageList
            if (bdy_arr.is_object() && bdy_arr.contains("messageList")) {
                for (auto& msg : bdy_arr["messageList"]) {
                    ChatMessage cm;
                    cm.message = msg.value("msg", "");

                    // Profile is a JSON string inside the message
                    if (msg.contains("profile") && msg["profile"].is_string()) {
                        auto profile = json::parse(msg["profile"].get<std::string>(), nullptr, false);
                        if (!profile.is_discarded())
                            cm.nickname = profile.value("nickname", "");
                    }

                    if (!cm.message.empty()) {
                        std::lock_guard<std::mutex> lock(msg_mutex_);
                        messages_.push_back(cm);
                        if (messages_.size() > 100) messages_.erase(messages_.begin());
                        if (callback_) callback_(cm);
                    }
                }
            }
            return;
        }

        for (auto& item : bdy_arr) {
            if (!item.is_object()) continue;

            // For cmd 93101, each item has msg, profile, etc.
            ChatMessage cm;
            cm.message = item.value("msg", "");

            if (item.contains("profile") && item["profile"].is_string()) {
                auto profile = json::parse(item["profile"].get<std::string>(), nullptr, false);
                if (!profile.is_discarded())
                    cm.nickname = profile.value("nickname", "");
            }

            if (item.contains("extras") && item["extras"].is_string()) {
                auto extras = json::parse(item["extras"].get<std::string>(), nullptr, false);
                if (!extras.is_discarded()) {
                    if (extras.contains("payAmount")) {
                        cm.is_donation = true;
                        cm.donation_amount = extras.value("payAmount", 0);
                    }
                }
            }

            if (!cm.message.empty()) {
                std::lock_guard<std::mutex> lock(msg_mutex_);
                messages_.push_back(cm);
                if (messages_.size() > 100) messages_.erase(messages_.begin());
                if (callback_) callback_(cm);
            }
        }
        return;
    }

    // Ping from server
    if (cmd == 0) {
        sendJson("{\"cmd\":10000,\"ver\":\"3\"}");
    }
}

void ChatClient::recvLoop() {
    dbg("chat: recv loop started");
    while (connected_) {
        std::string frame = recvWsFrame();
        if (!connected_) break;
        if (!frame.empty())
            handleMessage(frame);
    }
    dbg("chat: recv loop ended");
}

bool ChatClient::connect(const std::string& chat_channel_id) {
    chat_channel_id_ = chat_channel_id;

    // Random server 1-5
    int server = (rand() % 5) + 1;
    std::string host = "kr-ss" + std::to_string(server) + ".chat.naver.com";

    dbg("chat: connecting to WebSocket");
    if (!doTlsConnect(host, 443)) return false;
    if (!doWebSocketHandshake(host, "/chat")) return false;

    connected_ = true;

    // Send CONNECT
    json connect_msg;
    connect_msg["bdy"]["accTkn"] = nullptr;
    connect_msg["bdy"]["auth"] = "READ";
    connect_msg["bdy"]["devType"] = 2001;
    connect_msg["bdy"]["locale"] = "ko";
    connect_msg["bdy"]["timezone"] = "Asia/Seoul";
    connect_msg["bdy"]["uid"] = nullptr;
    connect_msg["cid"] = chat_channel_id;
    connect_msg["svcid"] = "game";
    connect_msg["cmd"] = 100;
    connect_msg["tid"] = 1;
    connect_msg["ver"] = "3";
    sendJson(connect_msg.dump());

    // Start recv thread
    recv_thread_ = std::thread([this]() { recvLoop(); });

    dbg("chat: connected");
    return true;
}

void ChatClient::disconnect() {
    connected_ = false;

    if (ssl_ctx_) {
        auto* ssl = static_cast<mbedtls_ssl_context*>(ssl_ctx_);
        mbedtls_ssl_close_notify(ssl);
        mbedtls_ssl_free(ssl);
        delete ssl;
        ssl_ctx_ = nullptr;
    }
    if (ssl_conf_) {
        auto* conf = static_cast<mbedtls_ssl_config*>(ssl_conf_);
        mbedtls_ssl_config_free(conf);
        delete conf;
        ssl_conf_ = nullptr;
    }
    if (net_ctx_) {
        auto* net = static_cast<mbedtls_net_context*>(net_ctx_);
        mbedtls_net_free(net);
        delete net;
        net_ctx_ = nullptr;
    }
    if (ctr_drbg_) {
        auto* c = static_cast<mbedtls_ctr_drbg_context*>(ctr_drbg_);
        mbedtls_ctr_drbg_free(c);
        delete c;
        ctr_drbg_ = nullptr;
    }
    if (entropy_) {
        auto* e = static_cast<mbedtls_entropy_context*>(entropy_);
        mbedtls_entropy_free(e);
        delete e;
        entropy_ = nullptr;
    }

    if (recv_thread_.joinable()) recv_thread_.join();
}

std::vector<ChatMessage> ChatClient::getMessages() {
    std::lock_guard<std::mutex> lock(msg_mutex_);
    return messages_;
}

#else
// Host stubs
bool ChatClient::connect(const std::string&) { return false; }
void ChatClient::disconnect() {}
std::vector<ChatMessage> ChatClient::getMessages() { return {}; }
bool ChatClient::doTlsConnect(const std::string&, int) { return false; }
bool ChatClient::doWebSocketHandshake(const std::string&, const std::string&) { return false; }
void ChatClient::sendWsFrame(const std::string&) {}
std::string ChatClient::recvWsFrame() { return ""; }
void ChatClient::sendJson(const std::string&) {}
void ChatClient::handleMessage(const std::string&) {}
void ChatClient::recvLoop() {}
#endif

}  // namespace chzzk
