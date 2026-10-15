#include "codet/core.hpp"
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
  codet::Cancellation c;
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
  } catch (const codet::Error &e) {
    std::cerr << "codet: " << e.what() << '\n';
    return e.code;
  } catch (const std::exception &e) {
    std::cerr << "codet: " << e.what() << '\n';
    return 1;
  }
}
