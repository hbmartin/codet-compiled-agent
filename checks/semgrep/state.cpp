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
