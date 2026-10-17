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
