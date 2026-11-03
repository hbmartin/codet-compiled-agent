# Prompt and evidence integrity follow-up

This follow-up starts at fetched main `bb9fa28de29cb2bcd19fa0ef36c053915b91b000`.
Implementation and validation were completed locally. At the user's later request, this branch is delivered as a ready follow-up PR; it is not merged and review threads are unchanged.

See the [final captured results, measurements, and limitations](20261103-4da7c4f-results.md) and the [immutable v4 audit bundle](20261103-4da7c4f/).

## Capture and replay

New captures use format v4. Production-content equality, validation inputs, and build inputs are separate identities. Capture records the exact child environment, canonical working directory, original cwd specification, executable spelling and identity, logs, outcomes, inputs, outputs, and producer links. Relative cwd is interpreted from the manifest root; relative executable and PATH entries are interpreted from that cwd. Shell credentials and undeclared preload/compiler/sanitizer settings are not inherited.

Prepare a new, single-attempt campaign directory. Interpreter overrides are explicit; the Python 3.9 check verifies major/minor rather than trusting a label.

```sh
build/lint-venv/bin/python scripts/evidence_campaign.py prepare \
  --root "$PWD" --folder build/prompt-evidence-integrity-NEW \
  --baseline bb9fa28de29cb2bcd19fa0ef36c053915b91b000 \
  --check-python "$PWD/build/lint-venv/bin/python" \
  --analysis-python "$PWD/build/lint-venv/bin/python" \
  --python39 /usr/bin/python3 \
  --clang-tidy /opt/homebrew/opt/llvm/bin/clang-tidy
build/lint-venv/bin/python scripts/validation_evidence.py capture \
  build/prompt-evidence-integrity-NEW/manifest.json \
  build/prompt-evidence-integrity-NEW/capture
build/lint-venv/bin/python scripts/validation_evidence.py bundle \
  build/prompt-evidence-integrity-NEW/manifest.json \
  build/prompt-evidence-integrity-NEW/capture \
  --output docs/validation/prompt-evidence-integrity-followup/NEW-RUN-ID
build/lint-venv/bin/python scripts/validation_evidence.py verify-bundle \
  docs/validation/prompt-evidence-integrity-followup/NEW-RUN-ID
```

Do not reuse a campaign directory, even after deleting config.json. Failed attempts retain their original outcomes and cannot satisfy acceptance. Build consumers verify producer outputs; the baseline tree is never overlaid. A separately declared common harness/support snapshot is compiled against each production library and its headers. Each campaign runs one warm-up and five alternating samples per variant, in each of two sets. No timing threshold determines correctness.

Offline audit reads only validated, contained bundle members. Original checkout paths are opaque provenance, not instructions to access the auditing machine. It checks exact declarations, digests, archived fingerprints, producer/outcome links, sidecar namespaces, and recomputed sample summaries. It does not rerun binaries or authenticate an author's identity. Historical formats receive explicit limited audit results and are ineligible for current acceptance; absent provenance is never backfilled.

CLI report publication and bundle publication refuse existing destinations atomically. The library's generic replacement interface remains available outside protected historical documentation.

## Analyzer review

Nonzero analyzer results remain observations. Strict old/new identities include locations and translation units. The required difference gate uses `checks/analyzer-relocations.json`, bound to both production maps. Its seven reviewed mappings retain the same diagnostic context; one maps the unchanged file-open flag expression into a local variable used for atomic CLOEXEC acquisition. New, ambiguous, or unregistered findings fail. This registry is not message-only matching.

## Historical helpers

Historical JSON, CSV, manifests, and source blobs are unchanged. Retained producer entry points now require explicit inputs/output locations, bounded commands, and fresh generated locations. Their original captured implementations remain available in historical source archives. Rejected lock consolidation is not part of this campaign.

The historical native compatibility wrapper accepts an explicit configuration and output:

```sh
build/lint-venv/bin/python docs/validation/diagnostic-record-lifecycle-followup-negative-controls.py \
  build/prompt-evidence-integrity-NEW/config.json --output /tmp/native-controls-NEW.json
```

Other retained historical experiments accept `--folder` and `--output`; output must be new. These produce historical replay records, not current capture-bound acceptance. Modern standalone producer computation also requires explicit `--output` and is labeled unbound. A captured producer must match the manifest's declared artifact and input set.

## Limits

Blocking TTY output and irrecoverable emergency delivery remain best effort. Pending submissions retain request identity across deferral; they are handed off only after presentation is attempted, and expired answers cannot become fresh user messages. Native inspection handles enforce the size observed at opening, not a content snapshot.

Ubuntu/container, PPC compiler, and native Tiger coverage must be reported from this campaign's actual availability checks. No old CI run substitutes for unpushed changes. The two previously documented Sonar local-tool path alerts remain outside this change: trusted local producer config paths are intentionally caller-selected, while untrusted offline bundle lookup paths are confined. No GitHub alert or thread is changed.
