"""Shared preparation workload registry and current measurement contract."""
METRIC_VERSION = 2
CHANGED_ADVISORY_CHECKS = 256
V2_ADVISORY_CHECKS = (128, 256) # Published version-2 positive-control contract.
WORKLOADS = ("allocation-positive", "preflight-checks", "saturated-count", "saturated-effective", "saturated-retained",
             "cli-preparation", "compatibility-preparation", "normalization")
