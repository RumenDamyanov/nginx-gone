#!/usr/bin/env bash
set -euo pipefail
# Negative / edge-case tests for nginx-gone module
# Tests: missing file, invalid regex, empty file, config validation.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
WORK_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
TEST_DIR=$(mktemp -d /tmp/nginx-gone-neg-XXXXXX)
PORT=8412
PASS=0
FAIL=0

cleanup() {
    if [ -f "$TEST_DIR/nginx.pid" ]; then
        kill -QUIT "$(cat "$TEST_DIR/nginx.pid")" 2>/dev/null || true
    fi
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

echo "=== nginx-gone negative tests ==="

MODULE_SO="$WORK_DIR/build/ngx_http_gone_module.so"
if [ ! -f "$MODULE_SO" ]; then
    echo "ERROR: Module not built. Run scripts/local-build.sh first."
    exit 1
fi

make_conf() {
    local map_file="$1"
    cat > "$TEST_DIR/nginx.conf" <<EOF
load_module $MODULE_SO;
events { worker_connections 64; }
http {
    gone_map_file $map_file;
    server {
        listen 127.0.0.1:$PORT;
        gone on;
        location / {
            default_type text/plain;
            return 200 "OK\n";
        }
    }
}
EOF
}

mkdir -p "$TEST_DIR/logs"

# --- Test 1: Missing map file should fail config test ---
echo ""
echo "--- Test 1: Missing map file ---"
make_conf "$TEST_DIR/nonexistent.map"
if nginx -t -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log" 2>/dev/null; then
    echo "  FAIL: nginx -t should have failed for missing map file"
    FAIL=$((FAIL + 1))
else
    echo "  PASS: nginx -t correctly rejected missing map file"
    PASS=$((PASS + 1))
fi

# --- Test 2: Invalid regex should fail config test ---
echo ""
echo "--- Test 2: Invalid regex in map file ---"
cat > "$TEST_DIR/bad-regex.map" <<EOF
/valid-uri 1
~[invalid( 1
EOF
make_conf "$TEST_DIR/bad-regex.map"
if nginx -t -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log" 2>/dev/null; then
    echo "  FAIL: nginx -t should have failed for invalid regex"
    FAIL=$((FAIL + 1))
else
    echo "  PASS: nginx -t correctly rejected invalid regex"
    PASS=$((PASS + 1))
fi

# --- Test 3: Empty map file should succeed ---
echo ""
echo "--- Test 3: Empty map file ---"
> "$TEST_DIR/empty.map"
make_conf "$TEST_DIR/empty.map"
if nginx -t -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log" 2>/dev/null; then
    echo "  PASS: nginx -t accepted empty map file"
    PASS=$((PASS + 1))
else
    echo "  FAIL: nginx -t should accept empty map file"
    FAIL=$((FAIL + 1))
fi

# --- Test 4: Comments-only map file should succeed ---
echo ""
echo "--- Test 4: Comments-only map file ---"
cat > "$TEST_DIR/comments.map" <<EOF
# This is a comment
# Another comment

# Blank lines above are fine too
EOF
make_conf "$TEST_DIR/comments.map"
if nginx -t -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log" 2>/dev/null; then
    echo "  PASS: nginx -t accepted comments-only map file"
    PASS=$((PASS + 1))
else
    echo "  FAIL: nginx -t should accept comments-only map file"
    FAIL=$((FAIL + 1))
fi

# --- Test 5: Empty map → all requests pass through ---
echo ""
echo "--- Test 5: Empty map passes all requests ---"
> "$TEST_DIR/empty.map"
make_conf "$TEST_DIR/empty.map"
nginx -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log"
sleep 0.5

actual=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/anything")
if [ "$actual" = "200" ]; then
    echo "  PASS: Empty map passes requests through (got $actual)"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Expected 200, got $actual"
    FAIL=$((FAIL + 1))
fi

# Stop nginx
if [ -f "$TEST_DIR/nginx.pid" ]; then
    kill -QUIT "$(cat "$TEST_DIR/nginx.pid")" 2>/dev/null || true
    sleep 0.5
fi

# --- Test 6: No gone_map_file directive → module inactive ---
echo ""
echo "--- Test 6: No map file directive ---"
cat > "$TEST_DIR/nginx.conf" <<EOF
load_module $MODULE_SO;
events { worker_connections 64; }
http {
    server {
        listen 127.0.0.1:$PORT;
        gone on;
        location / {
            default_type text/plain;
            return 200 "OK\n";
        }
    }
}
EOF
if nginx -t -c "$TEST_DIR/nginx.conf" -p "$TEST_DIR" -e "$TEST_DIR/error.log" 2>/dev/null; then
    echo "  PASS: Config without gone_map_file is valid"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Config without gone_map_file should be valid"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
if [ "$FAIL" -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
fi
echo "ALL TESTS PASSED"
