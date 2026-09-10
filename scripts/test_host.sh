#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
for test in protocol telemetry control_mode operating_mode setpoint; do
  c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
    "tests/grundfos_magna3/${test}_test.cpp" -o "$test_dir/$test"
  "$test_dir/$test"
  echo "PASS: $test"
done
python3 tests/grundfos_magna3/setpoint_sequence_test.py
