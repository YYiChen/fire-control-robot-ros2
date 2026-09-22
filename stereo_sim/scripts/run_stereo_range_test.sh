#!/usr/bin/env bash
# Validate all planned near-panel distances in isolated sequential runs.

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
for distance in 0.40 0.60 0.80; do
  echo "=== stereo distance ${distance} m ==="
  "${script_dir}/run_stereo_sim_test.sh" "$distance"
done

echo 'PASS: 0.40 m, 0.60 m, and 0.80 m stereo checks succeeded.'
