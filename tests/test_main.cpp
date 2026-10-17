#include "temp.hpp"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

int main(int argc, char **argv) {
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
