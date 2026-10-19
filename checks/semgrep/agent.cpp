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
