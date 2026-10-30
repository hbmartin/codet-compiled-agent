#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include <future>

namespace codet::detail {
// Construct before cancellation/quiescence; reporting and formatting cannot skip reset.
class TurnCancellationReset {
  Cancellation &cancel_;
  const bool &quitting_;

public:
  TurnCancellationReset(Cancellation &cancel, const bool &quitting) noexcept
      : cancel_(cancel), quitting_(quitting) {
    cancel_.requested = true;
  }
  TurnCancellationReset(const TurnCancellationReset &) = delete;
  TurnCancellationReset &operator=(const TurnCancellationReset &) = delete;
  ~TurnCancellationReset() {
    if (!quitting_)
      cancel_.requested = false;
  }
};
template <class Report> void secondary_diagnostic(Report report) noexcept {
  try {
    report();
  } catch (...) {
    // Secondary diagnostics must never replace the operation exception or terminate cleanup.
    return;
  }
}
// Bind the fallback explicitly to this invocation; preserve the public bad_alloc contract.
inline void context_diagnostic(const DiagnosticSink &sink, DiagnosticWriter &fallback,
                               RenderedTextView text) {
  const auto write = [&] { secondary_diagnostic([&] { (void)fallback.write({text.text}); }); };
  if (sink)
    try {
      sink(text.text);
      return;
    } catch (const std::bad_alloc &) {
      throw;
    } catch (...) {
      write(); // Keep this invocation's output gate, including exception-mask recovery.
      return;
    }
  write();
}
template <class Result, class Restore, class Drain, class Report>
[[noreturn]] void recover_turn(const std::exception_ptr &failure, Cancellation &cancel,
                               const bool &quitting, std::future<Result> &worker, Restore restore,
                               Drain drain, Report report) {
  TurnCancellationReset reset(cancel, quitting);
  while (worker.valid() &&
         worker.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
  }
  try {
    restore();
  } catch (...) {
    secondary_diagnostic([&] { report("Terminal restoration also failed"); });
  }
  try {
    drain();
  } catch (...) {
    secondary_diagnostic([&] { report("Worker events could not be drained during cleanup"); });
  }
  std::rethrow_exception(failure);
}
} // namespace codet::detail
