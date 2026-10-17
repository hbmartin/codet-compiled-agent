#pragma once
#include "codet/core.hpp"
#include "codet/output.hpp"
namespace codet {
struct ContextCapacity : Error {
  using Error::Error;
};
struct ContextMetrics {
  size_t scanned_bytes = 0, materializations = 0;
};
// Bound JSON string contents at a known nesting depth with one linear scan.
RenderedOutput context_excerpt(const std::string &, size_t encoded_bytes, unsigned depth = 1,
                               ContextMetrics * = nullptr);
std::vector<size_t> protected_history(const std::vector<Message> &);
std::vector<Message> project_history(std::vector<Message> &, size_t available, Tools &);
size_t minimal_batch_bytes(const std::vector<Message> &, const std::vector<ToolCall> &, Tools &);
void ensure_message_ids(std::vector<Message> &);
} // namespace codet
