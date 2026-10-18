#include "codet/output.hpp"
#include "process_helper.hpp"
#include "temp.hpp"
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>

std::string codet::test::executable;
int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  codet::test::executable = codet::fs::absolute(argv[0]).string();
  try {
    if (argc == 3 && std::string(argv[1]) == "--codet-catalog-lock-probe") {
      const int fd = ::open(argv[2], O_RDWR);
      if (fd < 0)
        return 2;
      struct flock lock{};
      lock.l_type = F_WRLCK;
      lock.l_whence = SEEK_SET;
      lock.l_len = 1;
      const auto result = ::fcntl(fd, F_SETLK, &lock);
      const auto error = errno;
      ::close(fd);
      return result == 0 ? 0 : (error == EACCES || error == EAGAIN ? 1 : 2);
    }
    if (argc == 5 && std::string(argv[1]) == "--codet-capture-helper") {
      codet::OutputSpool active(argv[2], 8, 8, 64);
      active.append("123456");
      active.flush();
      char token = active.bytes() == 6 ? 'y' : 'n';
      if (::write(std::stoi(argv[3]), &token, 1) != 1)
        return 1;
      if (!codet::test::read_token(std::stoi(argv[4]), token))
        return 1;
      active.close();
      return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--codet-crash-helper") {
      codet::StateStore store(argv[2], {}, argv[4]);
      auto session = store.load_id(argv[3], false);
      codet::Message assistant;
      assistant.role = "assistant";
      assistant.calls = {{"effect", "read_file", {{"path", "x"}}}};
      session.messages.push_back(assistant);
      store.append_messages(session, 0);
      store.tool_started(session.id, assistant.calls[0]);
      if (std::string(argv[5]) == "complete") {
        codet::Message result;
        result.role = "tool";
        result.tool_call_id = "effect";
        result.text = "completed once";
        store.complete_tool(session, assistant.calls[0], result);
      }
      ::_exit(0);
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  TempDirectory private_home;
  ::setenv("HOME", private_home.root.c_str(), 1);
  ::setenv("SHELL", "/bin/sh", 1);
  for (const auto *name :
       {"BASH_ENV", "ENV", "ZDOTDIR", "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "GOOGLE_API_KEY",
        "GOOGLE_GENERATIVE_AI_API_KEY", "OLLAMA_API_KEY", "SEARXNG_URL", "HTTP_PROXY",
        "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"})
    ::unsetenv(name);
  ::setenv("NO_PROXY", "127.0.0.1,localhost", 1);
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
