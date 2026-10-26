#include "codet/core.hpp"
#include "codet/detail/cli.hpp"
#include "codet/detail/diagnostic_stream.hpp"
#include "codet/detail/native_output.hpp"
#include <clocale>
#include <csignal>
#include <iostream>
namespace {
codet::Cancellation *cancellation = nullptr;
void interrupt(int) {
  if (cancellation)
    cancellation->requested.store(true);
}
} // namespace
int main(int argc, char **argv) {
  std::setlocale(LC_ALL, "");
  // Signal handlers remain installed through process teardown.
  static codet::Cancellation c;
  cancellation = &c;
  struct sigaction action {};
  action.sa_handler = interrupt;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);
  std::signal(SIGPIPE, SIG_IGN);
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  codet::detail::IoContext io(c);
  codet::detail::DiagnosticWriter errors(std::cerr, &io);
  try {
    return codet::detail::cli_main(args, c, io);
  } catch (const codet::ProviderError &e) {
    (void)errors.write({"codet: ", codet::detail::terminal_text(e.what())});
    (void)errors.write({codet::detail::terminal_text(e.body)});
    return e.code;
  } catch (const codet::Error &e) {
    (void)errors.write({"codet: ", codet::detail::terminal_text(e.what())});
    return e.code;
  } catch (const std::exception &e) {
    (void)errors.write({"codet: ", codet::detail::terminal_text(e.what())});
    return 1;
  }
}
