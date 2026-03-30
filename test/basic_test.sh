#!/usr/bin/env bash
set -euo pipefail
# Basic tests for nginx-gone module
# Tests exact URI matches, verifies non-matching URIs pass through,
# and verifies gone off disables checking.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
WORK_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
TEST_DIR=$(mktemp -d /tmp/nginx-gone-basic-XXXXXX)
PORT=8410
PASS=0
FAIL=0

cleanup() {
    if [ -f "$TEST_DIR/nginx.pid" ]; then
        kill -QUIT "$(cat "$TEST_DIR/nginx.pid")" 2>/dev/null || true
    fi
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

echo "=== nginx-gone basic tests ==="
echo "Work dir: $WORK_DIR"
echo "Test dir: $TEST_DIR"

# Check module .so exists
MODULE_SO="$WORK_DIR/build/ngx_http_gone_module.so"
if [ ! -f "$MODULE_SO" ]; then
    echo "ERROR: Module not built. Run scripts/local-build.sh first."
    exit 1
fi

# Prepare test directory
mkdir -p "$TEST_DIR/logs"
cp "$SCRIPT_DIR/fixtures/gone.map" "$TEST_DIR/gone.map"

# Generate nginx config from template
sed \
    -e "s|MODULE_PATH|$MODULE_SO|g" \
    -e "s|MAP_FILE|$TEST_DIR/gone.map|g" \
    -e "s|TEST_PORT|$PORT|g" \
    -e "s|TEST_DIR|$TEST_DIR|g" \
    "$SCRIPT_DIR/fixtures/nginx.conf.template" > "$TEST_DIR/nginx.conf"

# Test config validity
echo "Testing config..."
nginx -t -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log" 2>&1 || {
    echo "FAIL: nginx -t failed"
    cat "$TEST_DIR/error.log" 2>/dev/null
    exit 1
}

# Start nginx
nginx -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log"
sleep 0.5

check() {
    local desc="$1"
    local uri="$2"
    local expected="$3"
    local actual

    actual=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT$uri")
    if [ "$actual" = "$expected" ]; then
        echo "  PASS: $desc (got $actual)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected $expected, got $actual)"
        FAIL=$((FAIL + 1))
    fi
}

echo ""
echo "--- Exact match tests ---"
check "Exact match /old-page.html"          "/old-page.html"          "410"
check "Exact match /blog/2020/deleted-post" "/blog/2020/deleted-post" "410"
check "Exact match /products/old-item"      "/products/old-item"      "410"
check "Exact match /images/broken-banner.jpg" "/images/broken-banner.jpg" "410"
check "Exact match /about/old-team"         "/about/old-team"         "410"

echo ""
echo "--- Non-matching tests ---"
check "Non-match /index.html"      "/index.html"      "200"
check "Non-match /blog/2024/new"   "/blog/2024/new"   "200"
check "Non-match /products/active" "/products/active"  "200"
check "Non-match /"                "/"                 "200"

echo ""
echo "--- Gone off bypass ---"
check "Bypass /bypass"             "/bypass"           "200"
check "Bypass /bypass/anything"    "/bypass/anything"  "200"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    echo "SOME TESTS FAILED"
    cat "$TEST_DIR/error.log" 2>/dev/null
    exit 1
fi
echo "ALL TESTS PASSED"
