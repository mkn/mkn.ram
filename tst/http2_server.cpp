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
// Loopback test: our http2::Server on localhost with the repo's test certificate,
// exercised by our http2::Client - no network egress needed, so it runs in CI.
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "mkn/kul/log.hpp"
#include "mkn/ram/http2.hpp"

namespace {

using namespace mkn::ram::http2;

std::string PATTERN(size_t const& n) {
  std::string s(n, '\0');
  for (size_t i = 0; i < n; ++i) s[i] = static_cast<char>('a' + (i * 7 + i / 13) % 26);
  return s;
}

void CHECK(bool const& ok, std::string const& what) {
  if (!ok) KEXCEPTION("HTTP2 server test failed: " + what);
}

constexpr size_t BIG = 2 * 1024 * 1024;  // well over the 65535-byte default window
constexpr auto SLOW = std::chrono::milliseconds(100);

class TestServer : public Server {
 public:
  using Server::Server;
  Response respond(Request const& req) override {
    Response res;
    res.status(200);
    auto const& p = req.path();
    if (p == "/") {
      res.header("Content-Type", "text/plain");  // mixed case, must arrive lowercased
      res.header("Connection", "close");         // connection-specific, must be dropped
      res.body("hello h2");
    } else if (p == "/echo") {
      res.header("x-method", req.method());
      res.body(req.body());
    } else if (p == "/big") {
      res.body(PATTERN(BIG));
    } else if (p == "/headers") {
      auto it = req.headers().find("x-big");
      res.header("x-big", it == req.headers().end() ? "" : it->second);
    } else if (p == "/slow") {
      std::this_thread::sleep_for(SLOW);
      res.body("slow");
    } else if (p == "/throw") {
      KEXCEPTION("respond exception");
    } else {
      res.status(404);
    }
    return res;
  }
};

}  // namespace

int main() {
  try {
    constexpr uint16_t WORKERS = 4;
    TestServer server(0, mkn::kul::File("res/test/server.crt"),
                      mkn::kul::File("res/test/server.key"), WORKERS);
    server.init().start();
    auto const port = server.port();
    KLOG(INF) << "HTTP2 test server on port " << port;

    {
      Client client("localhost", port);
      Response res = client.request(Request("localhost", "/", port));
      CHECK(res.status() == 200, "GET / status " + std::to_string(res.status()));
      CHECK(res.body() == "hello h2", "GET / body");
      CHECK(res.header("content-type") == "text/plain", "GET / content-type");
      CHECK(!res.hasHeader("connection"), "connection-specific header not dropped");

      for (size_t i = 0; i < 20; ++i)  // sequential streams on one connection
        CHECK(client.request(Request("localhost", "/", port)).body() == "hello h2",
              "sequential request " + std::to_string(i));

      CHECK(client.request(Request("localhost", "/missing", port)).status() == 404, "404");
      CHECK(client.request(Request("localhost", "/throw", port)).status() == 500, "500");

      // client->server flow control: request body far larger than the server's window
      auto const body = PATTERN(BIG);
      res = client.request(Request("localhost", "/echo", port).withMethod("POST").withBody(body));
      CHECK(res.status() == 200, "POST /echo status");
      CHECK(res.header("x-method") == "POST", "POST /echo method");
      CHECK(res.body() == body, "POST /echo body (" + std::to_string(res.body().size()) + ")");

      // server->client flow control
      res = client.request(Request("localhost", "/big", port));
      CHECK(res.body() == PATTERN(BIG),
            "GET /big body (" + std::to_string(res.body().size()) + ")");

      // header blocks over the 16384-byte max frame size: CONTINUATION both ways
      auto const big = PATTERN(40000);
      res = client.request(Request("localhost", "/headers", port).withHeader("x-big", big));
      CHECK(res.header("x-big") == big,
            "CONTINUATION round trip (" + std::to_string(res.header("x-big").size()) + ")");
      KLOG(INF) << "single connection checks passed";
    }

    {  // many connections at once, served by a fixed pool rather than a thread each
      constexpr size_t CONNECTIONS = 8, REQUESTS = 5;
      std::atomic<size_t> ok{0};
      auto const start = std::chrono::steady_clock::now();
      std::vector<std::thread> clients;
      for (size_t i = 0; i < CONNECTIONS; ++i)
        clients.emplace_back([&]() {
          try {
            Client client("localhost", port);
            for (size_t r = 0; r < REQUESTS; ++r)
              if (client.request(Request("localhost", "/slow", port)).body() == "slow") ++ok;
          } catch (mkn::kul::Exception const& e) {
            KERR << e.stack();
          }
        });
      for (auto& t : clients) t.join();
      auto const elapsed = std::chrono::steady_clock::now() - start;
      auto const serial = SLOW * CONNECTIONS * REQUESTS;
      KLOG(INF) << "concurrent: " << ok << "/" << CONNECTIONS * REQUESTS << " in "
                << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                << "ms (serial would be " << serial.count() << "ms)";
      CHECK(ok == CONNECTIONS * REQUESTS, "concurrent requests");
      CHECK(elapsed < serial / 2, "requests were not served concurrently");
    }

    server.stop();
    KLOG(INF) << "HTTP2 server test passed";
  } catch (mkn::kul::Exception const& e) {
    KERR << e.stack();
    return 1;
  }
  return 0;
}
