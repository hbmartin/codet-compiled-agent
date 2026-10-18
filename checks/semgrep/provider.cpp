void request() {
  // ruleid: codet-public-reasoning-model-aware
  if (c.provider == "openai")
    body["reasoning"] = fields;
  // ruleid: codet-public-reasoning-model-aware
  if (!c.effort.empty() || c.provider == "openai")
    body["reasoning"] = fields;
  // ruleid: codet-public-reasoning-model-aware
  if (c.provider == "openai" || !c.effort.empty())
    body["reasoning"] = fields;
  // ok: codet-public-reasoning-model-aware
  if (public_responses_reasoning(c))
    body["reasoning"] = fields;
  // ok: codet-public-reasoning-model-aware
  if (!c.effort.empty())
    body["reasoning"] = fields;
}

void incomplete_event() {
  // ruleid: codet-incomplete-event-observed
  if (event == "response.incomplete")
    throw ProviderError("incomplete", 200, data);
  // ruleid: codet-incomplete-event-observed
  if (event == "response.incomplete") {
    diagnostic(data);
    throw ProviderError("incomplete", 200, data);
  }
  // ok: codet-incomplete-event-observed
  if (event == "response.incomplete") {
    saw = true;
    throw ProviderError("incomplete", 200, data);
  }
  // ok: codet-incomplete-event-observed
  if (event == "other")
    throw ProviderError("other", 200, data);
  // The unnamed event is parsed and marked observed before its type is inspected.
  saw = true;
  // ok: codet-incomplete-event-observed
  if (type == "response.incomplete")
    throw ProviderError("incomplete", 200, data);
}

void epoch_reply() {
  // ruleid: codet-epoch-empty-assistant
  auto bad = Json::array({{{"role", "assistant"}, {"content", result.message.replay}}});
  Json wire = Json::array();
  if (!result.message.replay.empty())
    // ok: codet-epoch-empty-assistant
    wire.push_back({{"role", "assistant"}, {"content", result.message.replay}});
}
