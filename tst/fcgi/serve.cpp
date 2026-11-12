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
// FastCGI server for res/test/nginx.sh, runs until interrupted
//   serve [serial|async] [port]

#include "mkn/ram/fcgi.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::sig_atomic_t volatile interrupted = 0;

mkn::ram::http::Response respond(mkn::ram::http::A1_1Request const& req) {
  using namespace std::chrono_literals;
  mkn::ram::http::Response res;
  if (req.path() == "/throw") throw std::runtime_error("expected test exception");
  if (req.path() == "/missing") {
    res.status(404);
    res.reason("Not Found");
    return res.withBody("missing").withDefaultHeaders();
  }
  if (req.path() == "/big") return res.withBody(std::string(1 << 20, 'z')).withDefaultHeaders();
  if (req.path() == "/sleep") std::this_thread::sleep_for(300ms);

  auto const find = [](auto const& map, std::string const& k) {
    auto const it = map.find(k);
    return it == map.end() ? std::string("-") : std::string(it->second);
  };
  res.header("X-Echo", "1");
  res.cookie("s", mkn::ram::http::Cookie("v"));
  return res
      .withBody(req.method() + "|" + req.path() + "|" + find(req.attributes(), "q") + "|" +
                find(req.headers(), "X-Test") + "|" + find(req.cookies(), "c") + "|" + req.host() +
                "|" + std::to_string(req.body().size()))
      .withDefaultHeaders();
}

}  // namespace

int main(int argc, char* argv[]) {
  bool const async = argc > 1 && std::string_view(argv[1]) == "async";
  auto const port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 9000);
  for (auto const sig : {SIGINT, SIGTERM}) std::signal(sig, [](int) { interrupted = 1; });

  std::unique_ptr<mkn::ram::fcgi::AServer> server;
  if (async)
    server = std::make_unique<mkn::ram::fcgi::AsyncServer>(port, 4);
  else
    server = std::make_unique<mkn::ram::fcgi::Server>(port);
  server->withResponse(respond);
  server->start();
  std::cout << "listening " << server->port() << std::endl;

  while (!interrupted && !server->exception())
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto const e = server->exception();
  server->stop().join();
  if (e) std::rethrow_exception(e);
  return 0;
}
