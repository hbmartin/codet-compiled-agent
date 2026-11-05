# Incomplete validation attempts

Strict campaign acceptance still requires every required check to pass. An offline
integrity audit does not change that policy. Historical bundles and helper scripts
retain their original contents and limitations.

When delivery is explicitly authorized with unavailable measurements, the maintained
assembler accepts only captured quiet-window failures in the measurement checks and
their blocked dependent benchmark check. Other required failures cannot be published
as an authorized incomplete attempt. Actual outcomes, logs, source/input identities,
and zero Release samples remain visible.

Use the validated checkout and its original capture directory, with Python 3.12+:

```sh
python3.12 scripts/evidence_attempt.py \
  --root "$PWD" \
  --campaign /absolute/path/to/frozen-campaign \
  --output docs/validation/queued-input-tiger-recovery-followup/RUN-ID \
  --unavailable-reason "Delivery authorized with benchmarks explicitly unavailable"
python3.12 scripts/evidence_attempt_audit.py \
  docs/validation/queued-input-tiger-recovery-followup/RUN-ID
```

The destination must not exist. Assembly stages a sibling directory, binds its
assembler/auditor helpers to validation fingerprints, verifies the staged inventory,
rechecks inputs, and publishes atomically. It reads no fixed temporary helper paths.
Inside a published bundle, `verify-attempt.py BUNDLE` runs its frozen offline auditor
after relocation; exit zero establishes integrity only, with `eligible_current=false`.
A failed stage never replaces an existing publication. The existing strict bundle
publisher remains the path for a fully successful campaign.

Tiger package determinism is scoped to repeated packaging of one frozen verified
build. Complete audit records retain original paths and configuration. No cross-
checkout byte-reproducibility or native Tiger runtime acceptance is implied.
