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
