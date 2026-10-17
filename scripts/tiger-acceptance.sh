#!/bin/sh
# Run by the owner on Tiger. No real credentials/network are used by default.
set -eu
umask 077
bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
report_dir=${CODET_ACCEPTANCE_REPORT:-"$PWD/codet-acceptance-$(date +%Y%m%d-%H%M%S)"}
mkdir -p "$report_dir"
test_root=$(mktemp -d "${TMPDIR:-/tmp}/codet-acceptance.XXXXXX")
trap 'rm -rf -- "$test_root"' EXIT HUP INT TERM
export CODET_HOME="$test_root/state"
mkdir -p "$test_root/workspace"
if [ ! -x /usr/bin/grep ]; then printf '%s\n' 'Missing /usr/bin/grep' >&2; exit 1; fi
printf '%s\n' 'needle' > "$test_root/workspace/grep-fixture"
/usr/bin/grep -n -E -e 'needle' -- "$test_root/workspace/grep-fixture" > "$report_dir/system-grep.txt"
uname -a > "$report_dir/system.txt"
sw_vers >> "$report_dir/system.txt" 2>/dev/null || true
sysctl hw.memsize hw.cpufrequency >> "$report_dir/system.txt" 2>/dev/null || true
otool -L "$bundle_dir/codet" > "$report_dir/dependencies.txt"
"$bundle_dir/codet_tests" --gtest_list_tests > "$report_dir/test-inventory.txt"
"$bundle_dir/codet" doctor -C "$test_root/workspace" > "$report_dir/doctor.json"
/usr/bin/time -l "$bundle_dir/codet" --version > "$report_dir/startup.txt" 2> "$report_dir/startup-resource.txt"
/usr/bin/time -l "$bundle_dir/codet_tests" --gtest_output="xml:$report_dir/tests.xml" > "$report_dir/tests.txt" 2> "$report_dir/test-resource.txt"
/usr/bin/time -l "$bundle_dir/codet_persistence_bench" 512 durable > "$report_dir/append-durable.jsonl" 2> "$report_dir/append-durable-resource.txt"
/usr/bin/time -l "$bundle_dir/codet_persistence_bench" 512 fast > "$report_dir/append-fast.jsonl" 2> "$report_dir/append-fast-resource.txt"
/usr/bin/time -l "$bundle_dir/codet_persistence_bench" captures > "$report_dir/capture-flush.jsonl" 2> "$report_dir/capture-flush-resource.txt"
printf '%s\n' 'Policies.* / DiagnosticPolicies.* / PromptPolicies.* cover observer-failure termination, private cleanup records, permission aliases/CRLF preflight, full hook budgets, diagnostic replay and prompt rollback.' > "$report_dir/process-permission-cases.txt"
printf '%s\n' 'OutputReview.* covers lazy/shared/ephemeral retention, original-byte previews, UTF-8, batching, quotas, malformed entries and buffer accounting.' > "$report_dir/output-cases.txt"
printf '%s\n' 'Followup.* covers exit observation/ownership, inherited signals, startup interruption, output captures/quotas/paging and provider selection.' > "$report_dir/followup-cases.txt"
printf '%s\n' 'Manual: both UIs, progress clear on success/error/Ctrl-C, SessionEnd diagnostics after terminal restore, portable search exclusions, capture paging.' >> "$report_dir/followup-cases.txt"
printf '%s\n' 'FinalReview.* / Sanitization.* cover literal workspace rules, timeout reasons, retained failed-job ownership, hook channel budgets/source matching, CRLF/lone-CR patches, exact IDs, expansion recovery and UTF-8 equivalence.' >> "$report_dir/followup-cases.txt"
printf '%s\n' 'Manual: live hook diagnostics in curses transcript, SessionStart startup/resume/fork context order, failed command expansion followed by a successful turn.' >> "$report_dir/followup-cases.txt"
case "${1:-}" in
  --desktop)
    export CODET_TEST_DESKTOP=1 CODET_DESKTOP_REPORT="$report_dir"
    /usr/bin/time -l "$bundle_dir/codet_tests" --gtest_filter=Contracts.NativeTigerDesktopAcceptance > "$report_dir/desktop.txt" 2> "$report_dir/desktop-resource.txt"
    ;;
  --tls)
    export CODET_TEST_TLS_URL=${CODET_TEST_TLS_URL:-https://auth.openai.com/.well-known/openid-configuration}
    "$bundle_dir/codet_tests" --gtest_filter=NetworkOptIn.TlsHandshake > "$report_dir/tls.txt" 2>&1
    ;;
  --subscription)
    if [ "$#" -ne 2 ]; then printf '%s\n' 'Usage: tiger-acceptance.sh --subscription /path/to/explicit-token-file' >&2; exit 2; fi
    "$bundle_dir/codet" login --import "$2" > "$report_dir/import.txt" 2>&1
    /usr/bin/time -l "$bundle_dir/codet" exec -C "$test_root/workspace" --json 'Reply with exactly: Tiger subscription transport works.' > "$report_dir/subscription.jsonl" 2> "$report_dir/subscription-resource.txt"
    ;;
  --api)
    provider=${CODET_TEST_PROVIDER:-openai}
    model=${CODET_TEST_MODEL:?Set CODET_TEST_MODEL explicitly for your account}
    /usr/bin/time -l "$bundle_dir/codet" exec -C "$test_root/workspace" --provider "$provider" --model "$model" --json 'Reply with exactly: Tiger API transport works.' > "$report_dir/api.jsonl" 2> "$report_dir/api-resource.txt"
    ;;
  '') ;;
  *) printf '%s\n' 'Options: --desktop | --tls | --subscription FILE | --api' >&2; exit 2 ;;
esac
printf '%s\n' "Acceptance reports: $report_dir" 'Runtime acceptance remains pending review of these reports.'
