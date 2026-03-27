#pragma once

#include <optional>
#include <string>
#include <vector>

namespace chzzk {

struct HttpHeader {
  std::string name;
  std::string value;
};

class HttpClient {
public:
  virtual ~HttpClient() = default;
  virtual std::optional<std::string> get(
      const std::string& url,
      const std::vector<HttpHeader>& headers = {}) = 0;
};

class FixtureHttpClient final : public HttpClient {
public:
  explicit FixtureHttpClient(std::string fixture_root);

  std::optional<std::string> get(
      const std::string& url,
      const std::vector<HttpHeader>& headers = {}) override;

private:
  std::string fixture_root_;
};

class HttpsHttpClient final : public HttpClient {
public:
  HttpsHttpClient() = default;

  std::optional<std::string> get(
      const std::string& url,
      const std::vector<HttpHeader>& headers = {}) override;
};

}  // namespace chzzk
