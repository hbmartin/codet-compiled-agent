#pragma once
#include "codet/core.hpp"
#include <ostream>
namespace codet {
enum class FileNoticeKind : uint8_t { repair_backup, tool_result };
// Allocate before publishing a file; delivery transfers its held path without allocation.
struct FileNotice {
  std::string path;
  FileNoticeKind kind;
  bool confirmed = false;
  FileNotice(std::string p, FileNoticeKind k) : path(std::move(p)), kind(k) {}
  const char *label() const noexcept;
  std::string text() const;

private:
  friend class FileDiagnosticQueue;
  FileNotice *next = nullptr;
  bool rendering_failed = false;
};
void emergency_file_diagnostic(const FileNotice &) noexcept;
// Attempt normal rendering once; preserve the full path through emergency output on failure.
void deliver_file_diagnostic(const FileNotice &, const DiagnosticSink &) noexcept;
// Clear stale errors and check both insertion and flushing before releasing a notice.
void write_file_diagnostic(std::ostream &, const std::string &);
const DiagnosticSink &default_file_diagnostic_sink() noexcept;
class FileDiagnosticReceiver {
public:
  virtual ~FileDiagnosticReceiver() = default;
  virtual void submit(std::unique_ptr<FileNotice>) noexcept = 0;
};
// Producers only transfer ownership. The terminal thread renders or defers each notice once.
class FileDiagnosticQueue final : public FileDiagnosticReceiver {
  std::atomic<FileNotice *> pending_{nullptr};
  FileNotice *deferred_ = nullptr, *deferred_tail_ = nullptr;
  FileNotice *take() noexcept;
  FileNotice *take_all() noexcept;
  void retain(std::unique_ptr<FileNotice>) noexcept;
  void render(const DiagnosticSink &, bool) noexcept;

public:
  ~FileDiagnosticQueue() override;
  void submit(std::unique_ptr<FileNotice>) noexcept override;
  void deliver(const DiagnosticSink &) noexcept;
  // Preview once, retaining the complete notice for replay after terminal restoration.
  void preview(const DiagnosticSink &) noexcept;
  // After restoration, render unfailed notices normally and use emergency output on failure.
  void finish(const DiagnosticSink &) noexcept;
  // Call after restoring the terminal. Emits full reversible paths without allocating.
  void flush() noexcept;
};
enum class DiagnosticLevel : uint8_t { info, warning, error };
inline std::string hook_diagnostic(const std::string &event, DiagnosticLevel level,
                                   const std::string &text) {
  if (event == "SessionEnd")
    return std::string(level == DiagnosticLevel::error     ? "SessionEnd hook cleanup failed: "
                       : level == DiagnosticLevel::warning ? "SessionEnd hook warning: "
                                                           : "SessionEnd hook: ") +
           text;
  return "Hook: " + text;
}
// One budget includes complete prefixes, UTF-8 repair, newlines and an omission notice.
class DiagnosticBuffer {
  size_t limit_, used_ = 0;
  bool omitted_ = false;
  std::vector<std::string> messages_;

public:
  explicit DiagnosticBuffer(size_t limit) : limit_(limit) {}
  static std::string notice() {
    return "[Additional diagnostics omitted or truncated]";
  }
  std::optional<std::string> add(const std::string &text) {
    const auto reserve = std::min(limit_, notice().size() + 1);
    const auto payload = limit_ - reserve;
    auto clean = safe_text(text);
    if (used_ >= payload) {
      omitted_ = true;
      return {};
    }
    const auto room = payload - used_ - 1;
    if (clean.size() > room) {
      clean = bounded_text(clean, room);
      omitted_ = true;
    }
    if (clean.empty()) {
      omitted_ = true;
      return {};
    }
    used_ += clean.size() + 1;
    messages_.push_back(clean);
    return clean;
  }
  bool omitted() const {
    return omitted_;
  }
  const std::vector<std::string> &messages() const {
    return messages_;
  }
  void clear() noexcept {
    messages_.clear();
    used_ = 0;
    omitted_ = false;
  }
  void flush(std::ostream &stream) {
    for (const auto &message : messages_)
      stream << message << '\n';
    if (omitted_ && limit_)
      stream << bounded_text(notice(), limit_ ? limit_ - 1 : 0) << '\n';
    clear();
  }
};
} // namespace codet
