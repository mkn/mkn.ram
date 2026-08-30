/**
Copyright (c) 2026, Philip Deegan.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Philip Deegan nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// Live smoke test: speaks real HTTP/2 (TLS+ALPN "h2") to a real server on the
// public internet, rather than a fixture - nghttp2.org is the reference HTTP/2
// implementation's own site.
#include "mkn/kul/log.hpp"
#include "mkn/ram/http2.hpp"

int main() {
  using namespace mkn::ram::http2;
  try {
    Client client("nghttp2.org");
    client.connect();
    KLOG(INF) << "ALPN negotiated h2, TLS handshake OK";

    Response res = client.request(Request("nghttp2.org", "/"));

    KLOG(INF) << "status: " << res.status();
    KLOG(INF) << "headers:";
    for (auto const& h : res.headers()) KLOG(INF) << "  " << h.first << ": " << h.second;
    KLOG(INF) << "body bytes: " << res.body().size();

    if (res.status() != 200)
      KEXCEPTION("HTTP2 test: expected status 200, got " + std::to_string(res.status()));
    if (res.body().empty()) KEXCEPTION("HTTP2 test: expected non-empty body");
    if (!res.hasHeader("content-type")) KEXCEPTION("HTTP2 test: expected content-type header");

    // second request over the same connection, exercising sequential stream reuse
    Response res2 = client.request(Request("nghttp2.org", "/blog/"));
    KLOG(INF) << "second request status: " << res2.status();
    if (res2.status() != 200 && res2.status() != 301 && res2.status() != 404)
      KEXCEPTION("HTTP2 test: unexpected status for second request: " +
                 std::to_string(res2.status()));

    client.close();

    // a body well over the 65535-byte default flow-control window, on a fresh
    // connection, to exercise stream- and connection-level WINDOW_UPDATE issuance
    Response big = get("www.cloudflare.com", "/");
    KLOG(INF) << "cloudflare.com status: " << big.status() << ", body bytes: " << big.body().size();
    if (big.status() != 200)
      KEXCEPTION("HTTP2 test: expected status 200 from cloudflare.com, got " +
                 std::to_string(big.status()));
    if (big.body().size() < _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_)
      KEXCEPTION("HTTP2 test: expected a body larger than the default flow-control window");

    KLOG(INF) << "HTTP2 client smoke test passed";
  } catch (mkn::kul::Exception const& e) {
    KERR << e.stack();
    return 1;
  }
  return 0;
}
