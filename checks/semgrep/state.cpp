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
