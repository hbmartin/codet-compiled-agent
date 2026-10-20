// Semgrep fixtures, intentionally uncompiled. Parsing and publication stay separate.
void backfill() {
  // ruleid: codet-backfill-inline-json
  remember_references(id, Json::parse(row).get<std::vector<Message>>());
  // ruleid: codet-backfill-inline-json
  remember_references(id, {Json::parse(row).get<Message>()});
  std::vector<Message> parsed;
  try {
    parsed = Json::parse(row).get<std::vector<Message>>();
  } catch (const Json::exception &) {
    return;
  }
  // ok: codet-backfill-inline-json
  remember_references(id, parsed);
  Message message;
  try {
    message = Json::parse(row).get<Message>();
  } catch (const Json::exception &) {
    return;
  }
  // ok: codet-backfill-inline-json
  remember_references(id, {message});
}

void StateStore::save_provider_state(const Session &session) {
  tx.commit();
  // ruleid: codet-provider-cache-owning-commit
  impl_->cache_provider(session, bytes);
  if (tx.owner)
    // ok: codet-provider-cache-owning-commit
    impl_->cache_provider(session, bytes);
}
void StateStore::append_messages(const Session &session, bool provider_state) {
  tx.commit();
  if (provider_state)
    // ruleid: codet-provider-cache-owning-commit
    impl_->cache_provider(session, bytes);
  if (provider_state && tx.owner)
    // ok: codet-provider-cache-owning-commit
    impl_->cache_provider(session, bytes);
}
void StateStore::replace_history(const Session &session) {
  tx.commit();
  // ruleid: codet-provider-cache-owning-commit
  impl_->cache_provider(session, bytes);
  if (tx.owner) {
    // ok: codet-provider-cache-owning-commit
    impl_->cache_provider(session, bytes);
  } else
    impl_->clear_provider_cache();
}

void StateStore::save_provider_state(const Session &session, const Hint &hint) {
  tx.commit();
  // ruleid: codet-provider-cache-owning-commit
  impl_->publish_provider(session, bytes);
  if (tx.owner)
    // ok: codet-provider-cache-owning-commit
    impl_->publish_provider(session, bytes);
}

struct ThrowingCache {
  // ruleid: codet-provider-cache-publication-noexcept
  void publish_provider(const Session &session) { cache_provider(session); }
};
struct TerminatingCache {
  // ruleid: codet-provider-cache-publication-noexcept
  void publish_provider(const Session &session) noexcept { cache_provider(session); }
};
struct RecoverableCache {
  // ok: codet-provider-cache-publication-noexcept
  void publish_provider(const Session &session) noexcept {
    try { cache_provider(session); } catch (...) { clear_provider_cache(); }
  }
};
Json StateStore::repair(const Session &session) {
  auto content = Json::parse(message.text);
  // ruleid: codet-repair-portable-content-only
  content["result_path"] = message.result_file;
  auto nested = Json::parse(envelope["content"]);
  // ruleid: codet-repair-portable-content-only
  nested["result_path"] = ref;
  // ok: codet-repair-portable-content-only
  portable_messages(session.messages, session.id);
}
void portable_messages(std::vector<Message> &messages) {
  // ok: codet-repair-portable-content-only
  metadata["result_path"] = message.result_file;
}
void load_cache() {
  // ruleid: codet-provider-cache-token-boundary
  impl_->publish_provider(session, bytes, impl_->provider_token(session.id));
  // ok: codet-provider-cache-token-boundary
  impl_->populate_provider(session, bytes);
  try {
    const auto token = impl_->provider_token(session.id);
    // ok: codet-provider-cache-token-boundary
    impl_->publish_provider(session, bytes, token);
  } catch (...) { impl_->clear_provider_cache(); }
}

Session StateStore::read_session(const std::string &id) {
  // ruleid: codet-load-cache-population-boundary
  const auto token = impl_->provider_token(id);
  // ruleid: codet-load-cache-population-boundary
  impl_->publish_provider(session, bytes, token);
  // ok: codet-load-cache-population-boundary
  impl_->populate_provider(session, bytes);
}
Session StateStore::load_id(const std::string &id) {
  // ruleid: codet-load-cache-population-boundary
  const auto token = impl_->provider_token(id);
  return session;
}
void required_write_validation() {
  // ok: codet-load-cache-population-boundary
  const auto token = impl_->provider_token(id);
}

void StateStore::reconcile(Session &session) {
  // ruleid: codet-reconcile-stable-checkpoint-view
  impl_->portable(session.messages, session.id);
  // ruleid: codet-reconcile-stable-checkpoint-view
  normalize_checkpoint_reference(stored, files, root, anchor);
  // ok: codet-reconcile-stable-checkpoint-view
  impl_->known_projections(stored, session.checkpoint_projection);
}
Session StateStore::fork(const Session &parent) {
  // ruleid: codet-state-filesystem-dependency
  NativeFileSystem files;
  // ok: codet-state-filesystem-dependency
  copy_image_history(root, *impl_->files, parent.id, child.id);
}
std::string StateStore::preserve_response(const Session &session, const Message &message) {
  // ruleid: codet-state-filesystem-dependency
  NativeFileSystem files;
  // ok: codet-state-filesystem-dependency
  impl_->files->atomic_write(path, bytes, true);
}
struct NativeFallback {
  // ok: codet-state-filesystem-dependency
  NativeFileSystem files;
};

struct PreparationFixture {
  PreparedTranscript prepare(const Session &s) {
    // ruleid: codet-transcript-preparation-is-pure
    s.checkpoint_projection = speculative;
    // ruleid: codet-transcript-preparation-is-pure
    s.checkpoint_projection->pending = next;
    // ok: codet-transcript-preparation-is-pure
    plan.committed = next;
    return plan;
  }
};
void StateStore::complete_tool(Session &s, const ToolCall &call, Message result) {
  impl_->require_owned_write();
  // ruleid: codet-prepared-transcript-publication
  append_messages(s, first);
  // ruleid: codet-prepared-transcript-publication
  tool_finished(s.id, call, result);
  // ok: codet-prepared-transcript-publication
  impl_->apply_prepared_transcript(s, prepared);
}
void StateStore::reconcile(Session &s, bool persist) {
  // ruleid: codet-prepared-transcript-publication
  save(s);
  // ruleid: codet-prepared-transcript-publication
  remember_references(s.id, s.messages);
  // ok: codet-prepared-transcript-publication
  impl_->apply_prepared_transcript(s, prepared);
}

// ruleid: codet-transcript-write-ownership
void StateStore::append_transcript(const Session &s) {
  auto prepared = impl_->prepare(s);
  impl_->apply_prepared_transcript(s, prepared);
}
// ok: codet-transcript-write-ownership
void StateStore::revise_transcript(const Session &s) {
  impl_->require_owned_write();
  auto prepared = impl_->prepare(s);
}
void StateStore::append_messages(const Session &s, size_t first) {
  // ruleid: codet-transcript-no-session-copy
  auto copy = s;
  append_transcript(copy, &copy, first);
}
void StateStore::replace_history(const Session &s) {
  // ok: codet-transcript-no-session-copy
  replace_transcript(s, nullptr);
}
void StateStore::reconcile(Session &destination, bool persist) {
  // ruleid: codet-transcript-no-session-copy
  auto session = destination;
  // ruleid: codet-recovery-prepared-result
  const auto result = safe_json(Json(recovery)).dump();
  // ok: codet-recovery-prepared-result
  const auto canonical = prepared.rows.at(position).payload;
}
Json StateStore::repair(...) {
  // ruleid: codet-session-scoped-preparation-version
  Statement version(db, "PRAGMA data_version");
  // ruleid: codet-session-scoped-preparation-version
  auto changes = sqlite3_total_changes(db);
  // ok: codet-session-scoped-preparation-version
  stale = !impl_->session_current(id, version, true);
}
void StateStore::append_transcript(...) {
  impl_->require_owned_write();
  {
    // ruleid: codet-transcript-selected-revision-guard
    Transaction tx(db);
    impl_->apply_prepared_transcript(s, prepared);
    tx.commit();
  }
}
void StateStore::revise_transcript(...) {
  impl_->require_owned_write();
  {
    // ok: codet-transcript-selected-revision-guard
    Transaction tx(db);
    if (!impl_->prepared_current(s.id, prepared))
      continue;
    impl_->apply_prepared_transcript(s, prepared);
    tx.commit();
  }
}
struct RecoveryPreparationFixture {
  PreparedTranscript prepare_recovered(...) {
    // ruleid: codet-recovery-prepared-result
    auto unsafe = safe_json(Json(result)).dump();
    // ok: codet-recovery-prepared-result
    auto canonical = plan.rows.at(position).payload;
  }
};
void StateStore::save_provider_state(const Session &selected) {
  auto publish = [&] {
    // ruleid: codet-provider-cache-owning-commit
    storage->publish_provider(selected);
  };
  if (feature_enabled && owning_transaction.owner) {
    // ok: codet-provider-cache-owning-commit
    storage->publish_provider(selected);
  }
}
