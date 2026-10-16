#include "codet/support.hpp"
#include <openssl/sha.h>

namespace codet {
std::string digest(const std::string &value) {
  unsigned char sum[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(), sum);
  std::string out;
  const char *hex = "0123456789abcdef";
  for (unsigned char byte : sum) {
    out += hex[byte >> 4];
    out += hex[byte & 15];
  }
  return out;
}
} // namespace codet
