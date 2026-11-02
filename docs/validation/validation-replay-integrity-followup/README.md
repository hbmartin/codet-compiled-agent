# Validation replay integrity follow-up

Fetched baseline: `7b8620f09dc4b42813f63e3048188984dfb033ca` (main after PR #51).

Version 3 captures bind actual build source trees, file inputs, external source
installation identities, configured tools and compilers, and produced outputs.
Fresh build directories prevent reuse of unbound historical test binaries.

Audit bundles archive declared source/configuration material by digest, retaining
only the current manifest's capture records. Measurement sidecars are namespaced
by producer. Offline verification checks complete inventory, provenance links,
archived inputs, actual statuses, raw sample summaries, and CSV contents. It does
not reproduce execution or include external SDK/tool/dependency installations.

Standalone compatibility producers use explicit config/output paths and label
results unbound. Native assertion controls remain separate from Python controls.
The process gate recognizes versioned/truncated analyzer identities and preserves
exact argument boundaries, with bounded inspection and explicit unavailable states.

The two Sonar CLI alerts refer to trusted local developer capabilities: explicit
command vectors and caller-selected publication paths. No production model tool or
sandbox boundary is added. Alerts and review threads remain unchanged.

Fresh validation, negative controls, samples, and platform limitations will be
published here after implementation and producer inputs are frozen. Historical
PR #49–51 JSON, CSV, and concrete command manifests remain unchanged.
