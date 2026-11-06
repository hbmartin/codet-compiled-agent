# Interactive command and package integrity follow-up

Baseline: `4d787a890c7ab06d6073d7ca024a38aaf60281ff` (merged PR #60).
Implementation uses an isolated worktree; the original checkout is preserved.

The follow-up freezes eligible interactive custom commands at Enter, treats
unknown interactive slash names as prose, preserves inactive queue ownership,
recognizes Rustls/BearSSL trust capabilities, aligns dependency-root defaults,
and binds package prefix decisions to archived recipe-manifest bytes. Attempt
auditing remains explicitly ineligible for full campaign acceptance when its
required measurement checks are unavailable.

The triage reproductions established idle path rejection, prompt-hook execution
before invalid command expansion, old-turn steering from compatibility submit,
real Rustls/BearSSL spelling mismatches, empty-root cwd selection, CMake ignoring
the provisioning override, stale full-lock cache metadata, leaked benchmark
startup patches, and Python 3.9's rejection of zip(strict=True). A transient
live-prefix fixture published alternate verified terminfo with the original
manifest before the fix; the regression requires the admitted original data.

Final captured outcomes, samples or explicit measurement unavailability, negative
controls, source identities, and offline-audit results are recorded in a new
immutable run directory. Historical validation bundles are unchanged. Host
fixtures do not establish native PowerPC/Tiger runtime acceptance.

Measurement eligibility is currently unavailable because native Darwin argv
inspection returns EINVAL. An inspection failure is preserved as source-bound
failure evidence, never as proof of a quiet environment. The full-record parsing
optimization is unretained without qualifying measurements.
