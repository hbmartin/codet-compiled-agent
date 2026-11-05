#pragma once
#include "codet/detail/local_command.hpp"
#include "codet/detail/transport_error.hpp"
#include <curl/curl.h>
#include <string_view>

namespace codet::detail {
enum class DirectoryTrust : uint8_t { present, absent, unknown };
// curl's public macro uses a signed shift. Keep an unsigned mask, checking its
// API value in preprocessing so strict signed-bitwise analysis stays meaningful.
#if CURL_VERSION_HTTPS_PROXY != (1U << 21U)
#error Unsupported curl HTTPS proxy feature bit
#endif
inline bool https_proxy_supported(unsigned int features) noexcept {
  return (features & (1U << 21U)) != 0;
}
struct TrustCapabilities {
  bool https_proxy = false;
  DirectoryTrust directories = DirectoryTrust::unknown;
};
inline DirectoryTrust directory_trust(std::string_view backend) noexcept {
  for (const std::string_view name :
       {"OpenSSL/", "LibreSSL/", "BoringSSL", "AWS-LC/", "GnuTLS/", "mbedTLS/", "wolfSSL/"})
    if (command_prefix(backend, name) &&
        (name.back() == '/' || backend.size() == name.size() || backend[name.size()] == ' ' ||
         backend[name.size()] == '/'))
      return DirectoryTrust::present;
  for (const std::string_view name :
       {"SecureTransport", "Secure Transport", "Schannel", "rustls/", "Rustls/"})
    if (command_prefix(backend, name) &&
        (name.back() == '/' || backend.size() == name.size() || backend[name.size()] == ' ' ||
         backend[name.size()] == '/'))
      return DirectoryTrust::absent;
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
