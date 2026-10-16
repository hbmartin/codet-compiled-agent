#pragma once
#include "codet/prompt.hpp"

namespace codet::test {
inline Json desktop_facts() {
  return {{"available", true},   {"backend", "test"}, {"accessibility", true},
          {"framebuffer", true}, {"clipboard", true}, {"applescript", true}};
}
inline size_t context_tokens(Tools &tools, const Config &config, size_t transcript_bytes) {
  const auto facts = tools.capabilities();
  const auto schemas = tools.schemas(facts);
  const auto system =
      assemble_prompt(config, schemas, facts, tools.instructions() + tools.dynamic_context());
  const auto budget = prompt_budget(config, system, schemas);
  return (budget.fixed + budget.output + transcript_bytes + 2) / 3;
}
} // namespace codet::test
