void request() {
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

void epoch_reply() {
  // ruleid: codet-epoch-empty-assistant
  auto bad = Json::array({{{"role", "assistant"}, {"content", result.message.replay}}});
  Json wire = Json::array();
  if (!result.message.replay.empty())
    // ok: codet-epoch-empty-assistant
    wire.push_back({{"role", "assistant"}, {"content", result.message.replay}});
}
