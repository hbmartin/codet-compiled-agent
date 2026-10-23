#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"
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
  try {
    return codet::cli_main(args, c);
  } catch (const codet::ProviderError &e) {
    (void)codet::detail::write_diagnostic(std::cerr, {"codet: ", e.what()});
    (void)codet::detail::write_diagnostic(std::cerr, {e.body});
    return e.code;
  } catch (const codet::Error &e) {
    (void)codet::detail::write_diagnostic(std::cerr, {"codet: ", codet::safe_text(e.what())});
    return e.code;
  } catch (const std::exception &e) {
    (void)codet::detail::write_diagnostic(std::cerr, {"codet: ", codet::safe_text(e.what())});
    return 1;
  }
}
