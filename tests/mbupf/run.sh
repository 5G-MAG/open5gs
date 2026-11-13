#!/bin/bash
# Runs the N4mb tests of the MB-UPF in a private user and network namespace (no sudo, no host interface is touched).
# Usage: tests/mbupf/run.sh <build dir> [test name filter]
set -e
here=$(cd "$(dirname "$0")" && pwd)
build=$(cd "${1:?build dir}" && pwd)
shift || true
exec unshare -Urn bash -c 'ip link set lo up && exec python3 -I "$0" "$@"' "$here/n4mb_test.py" "$build/src/upf/open5gs-upfd" "$@"
