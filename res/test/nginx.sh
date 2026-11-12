#!/usr/bin/env bash
# nginx -> mkn.ram FastCGI, against fcgi::Server and fcgi::AsyncServer
#
#   mkn build -dtOp test.fcgi.serve -a "-std=c++20 -fPIC" -l -pthread
#   res/test/nginx.sh
#
# Needs nginx, curl and ss, NGINX_BIN=/path/to/nginx overrides nginx on PATH
# (nginx itself reads NGINX).
set -euo pipefail
cd "$(dirname "$0")/../.."

NGINX_BIN=${NGINX_BIN:-$(command -v nginx || echo /usr/sbin/nginx)}
SERVE=${SERVE:-bin/test.fcgi.serve/mkn.ram}
HTTP_PORT=${HTTP_PORT:-18080}
FCGI_PORT=${FCGI_PORT:-19000}
URL="http://127.0.0.1:${HTTP_PORT}"
curl() { command curl --max-time 20 "$@"; }

TMP=$(mktemp -d)
FAILS=0
SERVER_PID= NGINX_PID=
stop() {
  [ -n "$NGINX_PID" ] && kill "$NGINX_PID" 2>/dev/null && wait "$NGINX_PID" 2>/dev/null || true
  [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null && wait "$SERVER_PID" 2>/dev/null || true
  NGINX_PID= SERVER_PID=
}
cleanup() {
  local code=$?
  stop
  if [ "$code" != 0 ] || [ "$FAILS" != 0 ]; then
    for f in error.log serve.log; do echo "== $f" && cat "$TMP/$f" 2>/dev/null || true; done
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

check() {  # name expected actual
  if [ "$2" == "$3" ]; then
    echo "PASS $MODE $1"
  else
    echo "FAIL $MODE $1: expected '$2', got '${3:0:200}'"
    FAILS=$((FAILS + 1))
  fi
}

wait_for() {  # file pattern
  for _ in $(seq 100); do
    grep -q "$2" "$1" 2>/dev/null && return 0
    sleep 0.1
  done
  echo "timed out waiting for '$2' in $1"
  cat "$1"
  exit 1
}

# the params nginx ships in fastcgi_params
cat > "$TMP/fastcgi_params" <<'PARAMS'
fastcgi_param  QUERY_STRING       $query_string;
fastcgi_param  REQUEST_METHOD     $request_method;
fastcgi_param  CONTENT_TYPE       $content_type;
fastcgi_param  CONTENT_LENGTH     $content_length;
fastcgi_param  SCRIPT_NAME        $fastcgi_script_name;
fastcgi_param  REQUEST_URI        $request_uri;
fastcgi_param  DOCUMENT_URI       $document_uri;
fastcgi_param  DOCUMENT_ROOT      $document_root;
fastcgi_param  SERVER_PROTOCOL    $server_protocol;
fastcgi_param  REQUEST_SCHEME     $scheme;
fastcgi_param  HTTPS              $https if_not_empty;
fastcgi_param  GATEWAY_INTERFACE  CGI/1.1;
fastcgi_param  SERVER_SOFTWARE    nginx/$nginx_version;
fastcgi_param  REMOTE_ADDR        $remote_addr;
fastcgi_param  REMOTE_PORT        $remote_port;
fastcgi_param  SERVER_ADDR        $server_addr;
fastcgi_param  SERVER_PORT        $server_port;
fastcgi_param  SERVER_NAME        $server_name;
fastcgi_param  REDIRECT_STATUS    200;
PARAMS

cat > "$TMP/nginx.conf" <<CONF
daemon off;
worker_processes 1;
pid $TMP/nginx.pid;
error_log $TMP/error.log warn;
events { worker_connections 64; }
http {
  access_log off;
  client_body_temp_path $TMP/body;
  fastcgi_temp_path $TMP/fastcgi;
  proxy_temp_path $TMP/proxy;
  uwsgi_temp_path $TMP/uwsgi;
  scgi_temp_path $TMP/scgi;
  client_max_body_size 16m;
  fastcgi_connect_timeout 5s;
  fastcgi_read_timeout 10s;
  fastcgi_send_timeout 10s;
  upstream fcgi {
    server 127.0.0.1:${FCGI_PORT};
    keepalive 8;
  }
  server {
    listen 127.0.0.1:${HTTP_PORT};
    location / {
      include $TMP/fastcgi_params;
      fastcgi_pass fcgi;
      fastcgi_keep_conn on;
    }
  }
}
CONF

upstream_conns() {
  ss -Htn state established "( dport = :${FCGI_PORT} )" | wc -l | tr -d ' '
}

for MODE in serial async; do
  : > "$TMP/error.log"
  "$SERVE" "$MODE" "$FCGI_PORT" > "$TMP/serve.log" 2>&1 &
  SERVER_PID=$!
  wait_for "$TMP/serve.log" "listening"
  "$NGINX_BIN" -p "$TMP" -c "$TMP/nginx.conf" -e "$TMP/error.log" &
  NGINX_PID=$!
  for _ in $(seq 100); do curl -so /dev/null "$URL/" && break; sleep 0.1; done

  check "GET" "GET|/echo/x|1|hi|3|127.0.0.1:${HTTP_PORT}|0" \
    "$(curl -sf -H 'X-Test: hi' -b 'c=3' "$URL/echo/x?q=1")"
  headers=$(curl -sf -D - -o /dev/null "$URL/")
  check "header" "1" "$(grep -ci '^X-Echo: 1' <<< "$headers")"
  check "cookie" "1" "$(grep -ci '^Set-Cookie: s=v' <<< "$headers")"
  check "POST 5MB" "POST|/post|-|-|-|127.0.0.1:${HTTP_PORT}|5242880" \
    "$(head -c 5242880 /dev/zero | curl -sf --data-binary @- "$URL/post")"
  check "GET 1MB" "1048576" "$(curl -sf "$URL/big" | wc -c | tr -d ' ')"
  check "404" "404" "$(curl -s -o /dev/null -w '%{http_code}' "$URL/missing")"
  check "500" "500" "$(curl -s -o /dev/null -w '%{http_code}' "$URL/throw")"

  codes=$(for i in $(seq 20); do curl -s -o /dev/null -w '%{http_code}\n' "$URL/seq/$i"; done | sort -u)
  check "20 sequential" "200" "$codes"
  check "upstream connection kept" "1" "$(upstream_conns)"

  start=$(date +%s%N)
  codes=$(for i in 1 2 3 4; do curl -s -o /dev/null -w '%{http_code}\n' "$URL/sleep" & done; wait)
  took=$((($(date +%s%N) - start) / 1000000))
  check "4 concurrent" "200" "$(sort -u <<< "$codes")"
  if [ "$MODE" == async ]; then  # 4 x 300ms on 4 workers, serial takes 1200ms+
    check "4 concurrent in parallel" "yes" "$([ "$took" -lt 900 ] && echo yes || echo "no, ${took}ms")"
  fi

  if grep -E '\[(error|crit|alert|emerg)\]' "$TMP/error.log"; then
    check "nginx error.log" "clean" "errors"
  fi
  stop
done

[ "$FAILS" == 0 ] || { echo "$FAILS failed"; exit 1; }
