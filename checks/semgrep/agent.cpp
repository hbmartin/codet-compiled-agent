void unsafe_response() {
  session.messages.push_back(response.message);
  // ruleid: codet-response-publication-rollback
  session.provider_state = provider_.conversation_state();
  try {
    // ruleid: codet-response-publication-rollback
    state_.append_messages(session, first, true, hint);
  } catch (const Error &) {
    session.messages.pop_back();
    throw;
  }
}
void guarded_response() {
  try {
    ResponseAppend publication(session);
    session.messages.push_back(response.message);
    // ok: codet-response-publication-rollback
    session.provider_state = provider_.conversation_state();
    // ok: codet-response-publication-rollback
    state_.append_messages(session, first, true, hint);
    publication.commit();
  } catch (...) {
    preserve_response(session, response.message);
    throw;
  }
}
void ordinary_transcript_append() {
  // ok: codet-response-publication-rollback
  state_.append_messages(session, first);
}
void budgets() {
  // ruleid: codet-runtime-aware-agent-budget
  auto ordinary = prompt_budget(config, system, schemas);
  // ruleid: codet-runtime-aware-agent-budget
  auto summary = prompt_budget(config, summary_system, Json::array()).available();
  // ok: codet-runtime-aware-agent-budget
  auto reserved = request_budget(config, system, schemas, execution);
}

void nested_publication() {
  auto attempt = [&] {
    // ruleid: codet-response-publication-rollback
    renamed.provider_state = transport.conversation_state();
  };
  {
    ResponseAppend renamed_guard(renamed);
    // ok: codet-response-publication-rollback
    renamed.provider_state = transport.conversation_state();
    renamed_guard.commit();
  }
}
void renamed_budget_arguments() {
  if (cancelled) return;
  // ruleid: codet-runtime-aware-agent-budget
  const auto missing_runtime = prompt_budget(other_config, other_system, other_schemas);
  // ok: codet-runtime-aware-agent-budget
  const auto guarded = request_budget(other_config, other_system, other_schemas, observations);
}
