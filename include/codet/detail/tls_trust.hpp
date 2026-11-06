#pragma once
#include "codet/detail/transport_error.hpp"
#include <cstdint>
#include <curl/curl.h>
#include <string_view>
#include <utility>

namespace codet::detail {
enum class DirectoryTrust : uint8_t { present, absent, unknown };
// Validate the native API without expanding its signed-shift macro into C++.
// Modeled analyzer headers may omit it; the assertion still rejects a missing
// or mismatched constant in a real compiler.
#if CURL_VERSION_HTTPS_PROXY != (1U << 21U)
static_assert(CURL_VERSION_HTTPS_PROXY == (1U << 21U), "Unsupported curl HTTPS proxy feature bit");
#endif
inline bool https_proxy_supported(unsigned int features) noexcept {
  return (features & (1U << 21U)) != 0;
}
struct TrustCapabilities {
  bool https_proxy = false;
  DirectoryTrust directories = DirectoryTrust::unknown;
};
inline DirectoryTrust directory_trust(std::string_view backend) noexcept {
  static constexpr std::pair<std::string_view, DirectoryTrust> backends[] = {
      {"OpenSSL/", DirectoryTrust::present},        {"LibreSSL/", DirectoryTrust::present},
      {"BoringSSL", DirectoryTrust::present},       {"AWS-LC/", DirectoryTrust::present},
      {"GnuTLS/", DirectoryTrust::present},         {"mbedTLS/", DirectoryTrust::present},
      {"wolfSSL/", DirectoryTrust::present},        {"SecureTransport", DirectoryTrust::absent},
      {"Secure Transport", DirectoryTrust::absent}, {"Schannel", DirectoryTrust::absent},
      {"rustls/", DirectoryTrust::absent},          {"Rustls/", DirectoryTrust::absent},
      {"rustls-ffi/", DirectoryTrust::absent},      {"crustls/", DirectoryTrust::absent},
      {"BearSSL", DirectoryTrust::absent}};
  for (const auto &entry : backends) {
    const auto name = entry.first;
    if (backend.size() >= name.size() && backend.compare(0, name.size(), name) == 0 &&
        (name.back() == '/' || backend.size() == name.size() || backend[name.size()] == ' ' ||
         backend[name.size()] == '/'))
      return entry.second;
  }
  return DirectoryTrust::unknown;
}
// The injected setter is also the capability regression seam; no ambient/test controls.
template <class Set>
void configure_tls_trust(const char *bundle, TrustCapabilities capabilities, Set set) {
  const auto checked = [&](CURLoption option, const char *value, const char *name,
                           bool directory = false) {
    const auto status = set(option, value);
    if (status == CURLE_OK)
      return;
    if (directory && capabilities.directories == DirectoryTrust::absent &&
        (status == CURLE_NOT_BUILT_IN || status == CURLE_UNKNOWN_OPTION))
      return; // This known backend has no directory store to disable.
    throw PermanentTransportError(std::string("Cannot set TLS trust ") + name + ": " +
                                  curl_easy_strerror(status));
  };
  checked(CURLOPT_CAINFO, bundle, "CAINFO");
  if (capabilities.https_proxy)
    checked(CURLOPT_PROXY_CAINFO, bundle, "PROXY_CAINFO");
  checked(CURLOPT_CAPATH, nullptr, "CAPATH", true);
  if (capabilities.https_proxy)
    checked(CURLOPT_PROXY_CAPATH, nullptr, "PROXY_CAPATH", true);
}
} // namespace codet::detail
