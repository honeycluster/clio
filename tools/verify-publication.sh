#!/usr/bin/env bash
# Run against an already configured build/dependency environment; never deploy.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${root}/build}"
jobs="${BUILD_JOBS:-4}"
repeats="${TEST_REPEATS:-3}"
for value in "$jobs" "$repeats"; do
    if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
        printf '%s\n' 'BUILD_JOBS and TEST_REPEATS must be positive integers' >&2
        exit 2
    fi
done

# Source-stage caches may have been configured with tests disabled.
cmake -S "$root" -B "$build_dir" -Dtests=ON -Dintegration_tests=OFF
cmake --build "$build_dir" --parallel "$jobs" --target clio_server clio_tests
"$build_dir/clio_tests" \
    --gtest_filter='FeedTransactionTest.*:FeedTrackableSignalTests.*:*ChannelSpawnTest*:*ChannelCallbackTest*:ClusterBackendTest.*' \
    --gtest_repeat="$repeats"
"$build_dir/clio_tests" --gtest_repeat="$repeats"
