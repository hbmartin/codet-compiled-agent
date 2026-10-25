#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <ostream>
namespace codet {
namespace detail {
void report_rendered_diagnostic(const DiagnosticSink &, RenderedTextView);
}
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
  static constexpr std::string_view omission = "[Additional diagnostics omitted or truncated]";
  size_t payload() const noexcept {
    return limit_ - std::min(limit_, omission.size() + 1);
  }
  std::optional<std::string> keep(std::string &&clean, bool truncated) {
    omitted_ |= truncated;
    if (clean.empty()) {
      omitted_ = true;
      return {};
    }
    used_ += clean.size() + 1;
    messages_.push_back(clean);
    return std::move(clean);
  }

public:
  explicit DiagnosticBuffer(size_t limit) : limit_(limit) {}
  static std::string notice() {
    return std::string(omission);
  }
  std::optional<std::string> add(const std::string &text) {
    if (used_ >= payload()) {
      omitted_ = true;
      return {};
    }
    auto clean = detail::render_terminal_text(text, payload() - used_ - 1);
    return keep(std::move(clean.text), clean.truncated);
  }
  std::optional<std::string> add_rendered(detail::RenderedTextView text) {
    if (used_ >= payload()) {
      omitted_ = true;
      return {};
    }
    const auto size = detail::rendered_prefix(text.text, payload() - used_ - 1);
    return keep(text.text.substr(0, size), size != text.text.size());
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
    detail::DiagnosticWriter writer(stream);
    flush_to(writer);
  }
  void flush_to(detail::DiagnosticWriter &writer) {
    for (const auto &message : messages_)
      (void)writer.write({message});
    if (omitted_ && limit_)
      (void)writer.write({bounded_text(notice(), limit_ ? limit_ - 1 : 0)});
    clear();
  }
};
} // namespace codet
