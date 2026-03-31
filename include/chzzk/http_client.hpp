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

  void setAuthCookies(const std::string& nid_aut, const std::string& nid_ses);
  std::string getAuthCookie() const;
  bool hasAuth() const { return !nid_aut_.empty() && !nid_ses_.empty(); }

  std::optional<std::string> get(
      const std::string& url,
      const std::vector<HttpHeader>& headers = {}) override;

private:
  std::string nid_aut_;
  std::string nid_ses_;
};

}  // namespace chzzk
