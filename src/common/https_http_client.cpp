#include "chzzk/http_client.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#ifdef __SWITCH__
#include <curl/curl.h>
#else
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"
#endif

namespace chzzk {
namespace {

struct ParsedUrl {
  std::string scheme;
  std::string host;
  std::string path_and_query;
  int port = 443;
};

std::optional<std::string> read_text_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::optional<ParsedUrl> parse_url(const std::string& url) {
  static const std::regex kPattern(R"(^(https?)://([^/:?#]+)(?::(\d+))?([^#]*)$)");
  std::smatch match;
  if (!std::regex_match(url, match, kPattern)) {
    return std::nullopt;
  }

  ParsedUrl parsed;
  parsed.scheme = match[1].str();
  parsed.host = match[2].str();
  parsed.port = match[3].matched ? std::stoi(match[3].str())
                                 : (parsed.scheme == "https" ? 443 : 80);
  parsed.path_and_query = match[4].str().empty() ? "/" : match[4].str();
  return parsed;
}

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  const size_t total_size = size * nmemb;
  auto* output = static_cast<std::string*>(userp);
  output->append(static_cast<const char*>(contents), total_size);
  return total_size;
}

#ifdef __SWITCH__
void append_http_log(const std::string& message) {
  std::ofstream log("sdmc:/switch/switch_chzzk.log", std::ios::app);
  if (log.is_open()) {
    log << message << '\n';
  }
}

std::string make_referer(const std::string& url) {
  const auto parsed = parse_url(url);
  if (!parsed.has_value()) {
    return "https://chzzk.naver.com/";
  }
  return parsed->scheme + "://" + parsed->host + "/";
}
#endif

#ifndef __SWITCH__
httplib::Headers to_headers(const std::vector<HttpHeader>& headers) {
  httplib::Headers out;
  for (const auto& header : headers) {
    out.insert({header.name, header.value});
  }
  return out;
}
#endif

}  // namespace

FixtureHttpClient::FixtureHttpClient(std::string fixture_root)
    : fixture_root_(std::move(fixture_root)) {}

std::optional<std::string> FixtureHttpClient::get(
    const std::string& url,
    const std::vector<HttpHeader>& headers) {
  (void)headers;

  const std::filesystem::path root(fixture_root_);

  if (url.find("/v1/lives") != std::string::npos) {
    return read_text_file(root / "live_list.json");
  }
  if (url.find("/live-detail") != std::string::npos) {
    return read_text_file(root / "live_detail.json");
  }
  if (url.find("master.m3u8") != std::string::npos) {
    return read_text_file(root / "live_master.m3u8");
  }

  return std::nullopt;
}

std::optional<std::string> HttpsHttpClient::get(
    const std::string& url,
    const std::vector<HttpHeader>& headers) {
#ifdef __SWITCH__
  CURL* curl = curl_easy_init();
  if (!curl) {
    return std::nullopt;
  }

  std::string response_body;
  char error_buffer[CURL_ERROR_SIZE] = {0};
  struct curl_slist* header_list = nullptr;
  for (const auto& header : headers) {
    const std::string line = header.name + ": " + header.value;
    header_list = curl_slist_append(header_list, line.c_str());
  }
  header_list = curl_slist_append(header_list, "Accept: */*");
  header_list = curl_slist_append(header_list, "Accept-Language: en-US,en;q=0.9");
  header_list = curl_slist_append(header_list, "Accept-Encoding: identity");
  header_list = curl_slist_append(header_list, "Connection: close");
  header_list = curl_slist_append(header_list, "Cache-Control: no-cache");
  header_list = curl_slist_append(header_list, ("Referer: " + make_referer(url)).c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   "Mozilla/5.0 (Nintendo Switch; WebApplet) "
                   "AppleWebKit/609.4.0 (KHTML, like Gecko) switch-chzzk/0.1");
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 32L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 32 * 1024L);
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L);
#ifdef CURLOPT_SSL_ENABLE_ALPN
  curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
#endif
#ifdef CURLOPT_SSL_ENABLE_NPN
  curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_NPN, 0L);
#endif
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

  const CURLcode result = curl_easy_perform(curl);
  long status_code = 0;
  char* effective_url = nullptr;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);

  if (result != CURLE_OK || status_code < 200 || status_code >= 300) {
    std::ostringstream message;
    message << "http: GET failed"
            << " result=" << static_cast<int>(result)
            << " curl=" << curl_easy_strerror(result)
            << " status=" << status_code
            << " effective_url=" << (effective_url ? effective_url : "(null)");
    if (error_buffer[0] != '\0') {
      message << " error_buffer=" << error_buffer;
    }
    append_http_log(message.str());
  } else {
    append_http_log("http: GET ok status=" + std::to_string(status_code) +
                    " bytes=" + std::to_string(response_body.size()));
  }

  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK || status_code < 200 || status_code >= 300) {
    return std::nullopt;
  }

  return response_body;
#else
  const auto parsed = parse_url(url);
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto request_headers = to_headers(headers);

  if (parsed->scheme == "https") {
    httplib::SSLClient client(parsed->host, parsed->port);
    client.set_follow_location(true);
    client.set_connection_timeout(5);
    client.set_read_timeout(5);
    client.enable_server_certificate_verification(true);

    const auto response = client.Get(parsed->path_and_query.c_str(), request_headers);
    if (!response || response->status < 200 || response->status >= 300) {
      return std::nullopt;
    }
    return response->body;
  }

  httplib::Client client(parsed->host, parsed->port);
  client.set_follow_location(true);
  client.set_connection_timeout(5);
  client.set_read_timeout(5);

  const auto response = client.Get(parsed->path_and_query.c_str(), request_headers);
  if (!response || response->status < 200 || response->status >= 300) {
    return std::nullopt;
  }
  return response->body;
#endif
}

}  // namespace chzzk
