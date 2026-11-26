#!/usr/bin/env bash
# Q2 ROS 2 Jazzy acceptance helper for AIM 2627 C++ OOP CW2.
# Run on Ubuntu 24.04 with ROS 2 Jazzy installed.
#
# Usage (from the repository root):
#   chmod +x docs/q2_ubuntu_verify.sh
#   ./docs/q2_ubuntu_verify.sh /path/to/aim-2627-cpp-oop-coursework
#
# If no path is given, the current directory is used as the repository root.
# Optional: Q2_TIMEOUT=90 sets the launch timeout in seconds (default 90).

set -euo pipefail

REPO_DIR="${1:-$(pwd)}"
TIMEOUT="${Q2_TIMEOUT:-90}"
LOG="/tmp/q2_launch_$$.log"
EXPECTED="/tmp/q2_expected_$$.txt"
ACTUAL="/tmp/q2_actual_$$.txt"

fail() {
    echo "FAIL: $*" >&2
    echo "Launch log kept at: $LOG" >&2
    exit 1
}

echo "== [1/4] Load ROS 2 Jazzy environment =="
[ -f /opt/ros/jazzy/setup.bash ] || fail "/opt/ros/jazzy/setup.bash not found; this script needs Ubuntu 24.04 + ROS 2 Jazzy."
# shellcheck disable=SC1091
source /opt/ros/jazzy/setup.bash
[ "${ROS_DISTRO:-}" = "jazzy" ] || fail "ROS_DISTRO is '${ROS_DISTRO:-unset}', expected 'jazzy'."
command -v colcon >/dev/null 2>&1 || fail "colcon not found; install with: sudo apt install python3-colcon-common-extensions"
echo "ROS_DISTRO=$ROS_DISTRO"

echo "== [2/4] cd ros2_ws && colcon build --symlink-install =="
[ -d "$REPO_DIR/ros2_ws" ] || fail "$REPO_DIR/ros2_ws does not exist."
cd "$REPO_DIR/ros2_ws"
colcon build --symlink-install

echo "== [3/4] source install/setup.bash =="
# shellcheck disable=SC1091
source install/setup.bash

echo "== [4/4] ros2 launch warehouse_dispatch warehouse_demo.launch.py =="
set +e
timeout "$TIMEOUT" ros2 launch warehouse_dispatch warehouse_demo.launch.py 2>&1 | tee "$LOG"
launch_rc=${PIPESTATUS[0]}
set -e
[ "$launch_rc" -eq 0 ] || fail "ros2 launch exited with $launch_rc (124 means timeout; check auto-exit)."

echo "== Check 1: exactly six core outputs, in order =="
cat > "$EXPECTED" <<'EOF'
ACCEPT O1 S1
ACCEPT O2 S2
REJECT O3
COMPLETE O1
REJECT O4
WAITED 1
EOF

# Strip ROS log prefixes such as "[INFO] [time] [warehouse_dispatcher]: ",
# keep only result lines, and trim trailing whitespace before comparison.
sed -E 's/^.*\]: //' "$LOG" \
    | grep -E '^(ACCEPT|REJECT|COMPLETE|WAITED) ' \
    | sed -E 's/[[:space:]]+$//' > "$ACTUAL" || true

if ! diff -u "$EXPECTED" "$ACTUAL"; then
    fail "Core output sequence differs from the Q2 acceptance table."
fi
echo "Output sequence OK."

echo "== Check 2: no BAD / BAD_COMPLETE lines =="
if grep -E '\]: BAD(_COMPLETE)?[[:space:]]' "$LOG"; then
    fail "Unexpected BAD or BAD_COMPLETE output."
fi
echo "No BAD lines."

echo "== Check 3: no leftover node processes =="
set +e
leftovers=$(timeout 10 ros2 node list 2>/dev/null | grep -E 'warehouse_(simulator|dispatcher)' || true)
set -e
[ -z "$leftovers" ] || fail "Nodes still registered after launch: $leftovers"
echo "No leftover nodes."

echo "PASS: Q2 local acceptance checks completed. Full log: $LOG"
