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

Campaign build locations use a conservative portable namespace: each path component
is Unicode-NFC normalized and case-folded for collision, overlap, reserved-name,
and control-prefix checks on every host. Case-sensitive hosts therefore reject
some locally distinct names intentionally. Submitted and resolved paths are both
checked before tool probes or campaign creation; actual I/O retains the selected
path spelling and resolved-containment rules. This is not an exact emulation of
all APFS or Tiger HFS+ filename rules.

Historical carrier extraction owns a unique sibling staging directory until its
full audit succeeds and publication is admitted exclusively. Cooperative SIGTERM
and Ctrl-C unwind that staging and restore signal handlers. No automatic stale-tree
sweep runs. SIGKILL, host failure, or an actual cleanup failure can leave staging;
inspect ownership and stop relevant operations before any manual deletion. A
secondary cleanup failure remains attached to the original error rather than
replacing it. Historical integrity never grants current campaign eligibility.
