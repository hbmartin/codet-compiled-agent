# Validation provenance follow-up

Baseline: `1842b76c783895253b1f1ade83c81e318c9e0339`, freshly fetched main after PR #50.
This follow-up changes developer tooling and tests, not production runtime behavior.

Version 2 captures bind each command to both production inputs and validation inputs.
Derived results carry their campaign, producing capture/check, and input hashes;
the capture records their finished output hashes. Complete analyzer execution is
validated separately from finding-related failure status. Benchmark medians, counts,
and work comparisons are derived from complete raw sample groups.

Publication stages a complete immutable bundle and verifies sources and sidecars
before exposing it. Previous PR #49/#50 reports, raw samples, and exact command
manifests remain historical and unchanged. Their executable compatibility entry
points delegate to maintained tools and require explicit output destinations.

Replay settings are resolved from `checks/validation-replay.json`; see the complete
prepare/capture/bundle commands in the repository README. No developer shell PATH
is copied into the replay template. Python 3.9 checks verify interpreter identity.
Process inspection preserves actual argument boundaries on Darwin and Linux, and
fails closed when eligibility cannot be established. No runtime dependency is added.

The two Sonar path alerts on the capture folder and output path do not identify an
application trust boundary: this local developer runner intentionally executes
manifest-provided command vectors and writes caller-selected report paths. It is
not a production agent tool or a sandbox for untrusted manifests. Confining report
paths to cwd would not make hostile command manifests safe. No alerts are dismissed
or quality gates weakened by this follow-up.

Fresh command outcomes, two sets of Release measurements, regression controls,
and host/platform limitations are recorded in the new run bundle linked below.
The previous LLVM 23 observations and Python 3.9 results are historical until
replaced by these newly captured outcomes; they are not assumed successful.
