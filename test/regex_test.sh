#!/usr/bin/env bash
set -euo pipefail
# Regex tests for nginx-gone module
# Tests case-sensitive (~) and case-insensitive (~*) regex patterns.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
WORK_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
TEST_DIR=$(mktemp -d /tmp/nginx-gone-regex-XXXXXX)
PORT=8411
PASS=0
FAIL=0

cleanup() {
    if [ -f "$TEST_DIR/nginx.pid" ]; then
        kill -QUIT "$(cat "$TEST_DIR/nginx.pid")" 2>/dev/null || true
    fi
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

echo "=== nginx-gone regex tests ==="

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
echo "--- Case-sensitive regex (~^/deprecated/) ---"
check "Match /deprecated/page"       "/deprecated/page"       "410"
check "Match /deprecated/sub/path"   "/deprecated/sub/path"   "410"
check "No match /Deprecated/page"    "/Deprecated/page"       "200"
check "No match /not-deprecated"     "/not-deprecated"        "200"

echo ""
echo "--- Case-insensitive regex (~*^/old-(.*)\.php\$) ---"
check "Match /old-page.php"          "/old-page.php"          "410"
check "Match /Old-Page.PHP"          "/Old-Page.PHP"          "410"
check "Match /OLD-STUFF.PHP"         "/OLD-STUFF.PHP"         "410"
check "No match /old-page.html"      "/old-page.html"         "410"
check "No match /new-page.php"       "/new-page.php"          "200"

echo ""
echo "--- Year-based archive regex (~^/archive/20[0-1][0-9]/) ---"
check "Match /archive/2010/post"     "/archive/2010/post"     "410"
check "Match /archive/2019/item"     "/archive/2019/item"     "410"
check "No match /archive/2020/post"  "/archive/2020/post"     "200"
check "No match /archive/2024/post"  "/archive/2024/post"     "200"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    echo "SOME TESTS FAILED"
    cat "$TEST_DIR/error.log" 2>/dev/null
    exit 1
fi
echo "ALL TESTS PASSED"
