#pragma once
#include "codet/detail/cleanup_diagnostics.hpp"
#include "codet/detail/cli_diagnostics.hpp"
#include "codet/detail/line_prompt.hpp"
#include "process_helper.hpp"

namespace codet::test {
inline int line_prompt_helper(const std::string &mode, int release, int notify) {
  if (mode != "approval" && mode != "question")
    return 2;
  Cancellation cancel;
  detail::IoContext io(cancel);
  detail::CliOutput output(io);
  detail::DiagnosticWriter errors(std::cerr, &io);
  detail::RequestInputController input;
  input.open({"s", "request", 1},
             mode == "approval" ? detail::RequestKind::approval : detail::RequestKind::question);
  struct stat terminal {};
  if (::fstat(STDOUT_FILENO, &terminal) < 0)
    return 3;
  const std::string prompt = "codet> ";
  detail::LinePrompt line(input, output, io, prompt, terminal);
  line.acquire();
  line.flush();
  line.append('y');
  if (::write(notify, "r", 1) != 1)
    return 4;
  char token;
  if (!read_token(release, token))
    return 5;
  (void)errors.write({"warning"});
  detail::CliDiagnostics diagnostics(std::cerr, &io);
  auto notice = std::make_unique<FileNotice>("/complete/path", FileNoticeKind::tool_result);
  notice->confirmed = true;
  diagnostics.files->submit(std::move(notice));
  diagnostics.deliver_files();
  line.flush();
  // Production closes the request and releases repaint ownership before the cleanup drain.
  auto worker = std::async(std::launch::async, [] { return 0; });
  const auto original = std::make_exception_ptr(Error("original"));
  const bool quitting = false;
  try {
    detail::recover_turn(
        original, cancel, quitting, worker,
        [&] {
          input.close();
          line.release();
        },
        [] { throw Error("drain failed"); },
        [&](std::string_view text) { (void)errors.write({text}); });
  } catch (...) {
    if (std::current_exception() != original || cancel.requested)
      return 6;
  }
  line.flush();
  if (::write(notify, "d", 1) != 1 || !read_token(release, token))
    return 7;
  line.submitted();
  return input.take().text == "y" ? 0 : 8;
}
} // namespace codet::test
