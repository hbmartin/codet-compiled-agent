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
bool reduce_completed_result(Message &, const std::string &, Tools &);
std::optional<size_t> fit_result_preview(Message &, Message, size_t, const std::string &, Tools &);
std::vector<size_t> batch_result_reserves(const std::vector<ToolCall> &, Tools &);
size_t minimal_batch_bytes(const std::vector<Message> &, const std::vector<ToolCall> &, Tools &);
void ensure_message_ids(std::vector<Message> &);
} // namespace codet
