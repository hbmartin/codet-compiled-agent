// Semgrep fixtures, intentionally uncompiled. Runtime tests validate helper semantics.
Json Tools::execution_state(const std::vector<Message> &messages) const {
  // ruleid: codet-checkpoint-direct-status
  processes_.status("unguarded");
  // ok: codet-checkpoint-direct-status
  return execution_fact(1, session);
}
Json Tools::execution_fact(int id, const ExecSession &session) const {
  try {
    // ok: codet-checkpoint-direct-status
    return processes_.status(session.process);
  } catch (const std::exception &error) {
    if (fatal(std::current_exception())) throw;
    return {{"state", "unknown"}};
  }
}
