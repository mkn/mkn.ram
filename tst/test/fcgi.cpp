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
// FastCGI servers over loopback, with a minimal client written against the record codec.

#ifndef _WIN32

#include "mkn/ram/fcgi.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace mkn::ram;
using namespace std::chrono_literals;

void fail(std::string const& what) {
  std::cerr << "FAIL fcgi " << what << std::endl;
  std::_Exit(1);
}

void expect(bool const ok, std::string const& what) {
  if (!ok) fail(what);
}

struct Reply {
  std::string out;
  int status = -1;  // END_REQUEST protocolStatus
  std::uint8_t type = 0;
  std::string content;  // last management record
};

class Client {
 public:
  explicit Client(std::uint16_t const port) : fd(::socket(AF_INET, SOCK_STREAM, 0)) {
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0) fail("connect");
  }
  ~Client() { ::close(fd); }

  Client& send(std::string const& s) {
    for (std::size_t sent = 0; sent < s.size();) {
      auto const n = ::send(fd, s.data() + sent, s.size() - sent, MSG_NOSIGNAL);
      if (n <= 0) fail("send");
      sent += static_cast<std::size_t>(n);
    }
    return *this;
  }

  // reads until `ends` records ending a request or a management reply
  std::map<std::uint16_t, Reply> read(std::size_t ends = 1) {
    std::map<std::uint16_t, Reply> replies;
    char buf[4096];
    while (ends) {
      fcgi::record::Record r;
      while (ends && parser.next(r)) {
        auto& reply = replies[r.id];
        if (r.type == fcgi::type::STDOUT)
          reply.out += r.content;
        else if (r.type == fcgi::type::END_REQUEST) {
          reply.status = static_cast<std::uint8_t>(r.content[4]);
          --ends;
        } else if (r.id == fcgi::NULL_REQUEST_ID) {
          reply.type = r.type;
          reply.content = r.content;
          --ends;
        }
      }
      if (!ends) break;
      auto const n = ::recv(fd, buf, sizeof(buf), 0);
      if (n <= 0) fail("read, connection closed or timed out");
      parser.feed(buf, static_cast<std::size_t>(n));
    }
    return replies;
  }

  bool closed() {
    char c;
    return ::recv(fd, &c, 1, 0) == 0;
  }

 private:
  int fd;
  fcgi::record::Parser parser;
};

std::string request(std::uint16_t const id, fcgi::Params const& params, std::string const& in = "",
                    bool const keep = false) {
  std::string out;
  fcgi::record::begin(out, id, fcgi::role::RESPONDER, keep ? fcgi::KEEP_CONN : 0);
  fcgi::record::append(out, fcgi::type::PARAMS, id, fcgi::record::encode(params));
  fcgi::record::append(out, fcgi::type::PARAMS, id, {});
  if (!in.empty()) fcgi::record::append(out, fcgi::type::STDIN, id, in);
  fcgi::record::append(out, fcgi::type::STDIN, id, {});
  return out;
}

fcgi::Params get(std::string const& uri) {
  return {{"REQUEST_METHOD", "GET"}, {"REQUEST_URI", uri}};
}

bool has(std::string const& s, std::string const& part) {
  return s.find(part) != std::string::npos;
}

http::Response respond(http::A1_1Request const& req) {
  if (req.path() == "/throw") throw std::runtime_error("expected test exception");
  if (req.path() == "/sleep") std::this_thread::sleep_for(300ms);
  auto const header = [&](std::string const& k) {
    auto const it = req.headers().find(k);
    return it == req.headers().end() ? std::string("-") : it->second;
  };
  auto const attribute = [&](std::string const& k) {
    auto const it = req.attributes().find(k);
    return it == req.attributes().end() ? std::string("-") : it->second;
  };
  auto const cookie = [&](std::string const& k) {
    auto const it = req.cookies().find(k);
    return it == req.cookies().end() ? std::string("-") : it->second;
  };
  http::Response res;
  res.header("X-Test", "1");
  res.cookie("s", http::Cookie("v"));
  res.withBody(req.method() + "|" + req.path() + "|" + attribute("a") + "|" +
               header("Accept-Encoding") + "|" + cookie("c") + "|" + req.host() + "|" + req.ip() +
               "|" + std::to_string(req.port()) + "|" + std::to_string(req.body().size()))
      .withDefaultHeaders();
  return res;
}

std::string mpxs(std::uint16_t const port) {
  Client c(port);
  std::string out;
  fcgi::record::append(out, fcgi::type::GET_VALUES, 0,
                       fcgi::record::encode({{"FCGI_MPXS_CONNS", ""}, {"FCGI_MAX_CONNS", ""}}));
  auto const reply = c.send(out).read()[0];
  expect(reply.type == fcgi::type::GET_VALUES_RESULT, "GET_VALUES_RESULT");
  auto const values = fcgi::record::decode(reply.content);
  expect(values.count("FCGI_MAX_CONNS"), "FCGI_MAX_CONNS");
  return values.at("FCGI_MPXS_CONNS");
}

void test_server() {
  fcgi::Server server(0);
  server.withResponse(respond);
  server.start();
  auto const port = server.port();

  {  // params to request, response to CGI, closed without KEEP_CONN
    Client c(port);
    auto const reply = c.send(request(1, {{"REQUEST_METHOD", "GET"},
                                          {"REQUEST_URI", "/p/x?a=1&b"},
                                          {"QUERY_STRING", "a=1&b"},
                                          {"HTTP_HOST", "example.com"},
                                          {"HTTP_ACCEPT_ENCODING", "gzip"},
                                          {"HTTP_COOKIE", "c=3; d=4"},
                                          {"REMOTE_ADDR", "10.0.0.1"},
                                          {"REMOTE_PORT", "1234"},
                                          {"LONG", std::string(300, 'x')}}))
                           .read()[1];
    expect(reply.status == fcgi::status::REQUEST_COMPLETE, "REQUEST_COMPLETE");
    expect(reply.out.starts_with("Status: 200 OK\r\n"), "status line");
    expect(has(reply.out, "\r\nX-Test: 1\r\n"), "header");
    expect(has(reply.out, "\r\nSet-Cookie: s=v; "), "cookie");
    expect(!has(reply.out, "Connection:"), "hop-by-hop header dropped");
    expect(reply.out.ends_with("\r\n\r\nGET|/p/x|1|gzip|3|example.com|10.0.0.1|1234|0"), "body");
    expect(c.closed(), "closed without KEEP_CONN");
  }
  {  // STDIN over several records, then a request sent a byte at a time
    Client c(port);
    fcgi::Params params{{"REQUEST_METHOD", "POST"}, {"SCRIPT_NAME", "/post"}};
    auto const big = c.send(request(1, params, std::string(200000, 'y'), true)).read()[1];
    expect(big.out.ends_with("POST|/post|-|-|-|||0|200000"), "large STDIN");
    for (char const ch : request(2, params, "abc")) c.send(std::string(1, ch));
    expect(c.read()[2].out.ends_with("|3"), "byte at a time");
  }
  {  // KEEP_CONN
    Client c(port);
    for (std::uint16_t id = 1; id <= 3; ++id)
      expect(
          c.send(request(id, get("/k"), "", true)).read()[id].out.ends_with("GET|/k|-|-|-|||0|0"),
          "KEEP_CONN request " + std::to_string(id));
  }
  {  // a throwing responder is a 500
    Client c(port);
    expect(c.send(request(1, get("/throw"))).read()[1].out.starts_with("Status: 500"), "500");
  }
  {  // roles, multiplexing, unknown management records
    Client c(port);
    std::string out;
    fcgi::record::begin(out, 1, fcgi::role::AUTHORIZER, fcgi::KEEP_CONN);
    expect(c.send(out).read()[1].status == fcgi::status::UNKNOWN_ROLE, "UNKNOWN_ROLE");

    out.clear();
    fcgi::record::begin(out, 2, fcgi::role::RESPONDER, fcgi::KEEP_CONN);
    fcgi::record::begin(out, 3, fcgi::role::RESPONDER, fcgi::KEEP_CONN);
    expect(c.send(out).read()[3].status == fcgi::status::CANT_MPX_CONN, "CANT_MPX_CONN");
    out = request(2, get("/after"), "", true).substr(fcgi::HEADER_LEN * 2);  // less BEGIN
    expect(c.send(out).read()[2].out.ends_with("GET|/after|-|-|-|||0|0"), "after CANT_MPX_CONN");

    out.clear();
    fcgi::record::append(out, 20, 0, {});
    auto const unknown = c.send(out).read()[0];
    expect(unknown.type == fcgi::type::UNKNOWN_TYPE && unknown.content[0] == 20, "UNKNOWN_TYPE");
  }
  expect(mpxs(port) == "0", "Server FCGI_MPXS_CONNS");

  Client idle(port);  // stop closes open connections
  {
    std::string out;
    fcgi::record::append(out, fcgi::type::GET_VALUES, 0, {});
    expect(idle.send(out).read()[0].type == fcgi::type::GET_VALUES_RESULT, "idle accepted");
  }
  server.stop().join();
  expect(idle.closed(), "closed on stop");
  if (server.exception()) std::rethrow_exception(server.exception());
  std::cout << "PASS fcgi::Server" << std::endl;
}

void test_async_server() {
  fcgi::AsyncServer server(0, 4);
  server.withResponse(respond);
  server.start();
  auto const port = server.port();

  expect(mpxs(port) == "1", "AsyncServer FCGI_MPXS_CONNS");

  {  // interleaved requests on one connection
    Client c(port);
    std::string out;
    fcgi::record::begin(out, 1, fcgi::role::RESPONDER, fcgi::KEEP_CONN);
    fcgi::record::begin(out, 2, fcgi::role::RESPONDER, fcgi::KEEP_CONN);
    for (std::uint16_t const id : {std::uint16_t{2}, std::uint16_t{1}}) {
      fcgi::record::append(out, fcgi::type::PARAMS, id,
                           fcgi::record::encode(get(id == 1 ? "/sleep" : "/two")));
      fcgi::record::append(out, fcgi::type::PARAMS, id, {});
      fcgi::record::append(out, fcgi::type::STDIN, id, {});
    }
    auto replies = c.send(out).read(2);
    expect(replies[1].out.ends_with("GET|/sleep|-|-|-|||0|0"), "multiplexed 1");
    expect(replies[2].out.ends_with("GET|/two|-|-|-|||0|0"), "multiplexed 2");
  }
  {  // 4 slow requests on 4 workers run together
    auto const start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
      threads.emplace_back([port]() {
        Client c(port);
        expect(c.send(request(1, get("/sleep"))).read()[1].status == 0, "concurrent request");
      });
    for (auto& t : threads) t.join();
    auto const took = std::chrono::steady_clock::now() - start;
    expect(took < 900ms,
           "concurrent requests took " +
               std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(took).count()) +
               "ms");
  }

  server.stop().join();
  if (server.exception()) std::rethrow_exception(server.exception());
  std::cout << "PASS fcgi::AsyncServer" << std::endl;
}

}  // namespace

void test_fcgi() {
  std::thread([]() {
    std::this_thread::sleep_for(60s);
    fail("timed out");
  }).detach();
  test_server();
  test_async_server();
}

#else

void test_fcgi() {}

#endif  // _WIN32
