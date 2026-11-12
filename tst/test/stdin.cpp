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
// Servers must not poll fds they don't own. Here stdin is a pipe whose writer is closed, as
// on CI runners, which polls POLLHUP forever: a server watching fd 0 never serves anyone.
// Linked into test.cpp, POSIX only, run last as it replaces the process stdin.

#ifndef _WIN32

#include "mkn/kul/log.hpp"
#include "mkn/ram/http.hpp"
#include "mkn/ram/https.hpp"

#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <string>
#include <thread>

namespace {

constexpr std::uint16_t HTTP_PORT = 8890, HTTPS_PORT = 8891;
constexpr auto TIMEOUT = std::chrono::seconds(10);

void close_stdin_pipe() {
  int fds[2];
  if (pipe(fds) != 0) KEXCEPTION("pipe failed");
  ::close(fds[1]);
  if (fds[0] == STDIN_FILENO) return;  // stdin was already closed
  if (dup2(fds[0], STDIN_FILENO) < 0) KEXCEPTION("dup2 failed");
  ::close(fds[0]);
}

mkn::ram::http::Response respond(mkn::ram::http::A1_1Request const&) {
  return mkn::ram::http::Response{}.withBody("HELLO WORLD").withDefaultHeaders();
}

// a hung server never answers, so the request runs detached and the process exits on timeout
template <class Get>
void check(std::string const& name, std::uint16_t const port) {
  std::promise<std::string> body;
  auto result = body.get_future();
  std::thread([&body, port]() {
    try {
      Get("localhost", "", port)
          .withResponse([&](mkn::ram::http::Response const& r) { body.set_value(r.body()); })
          .send();
    } catch (...) {
      body.set_exception(std::current_exception());
    }
  }).detach();
  if (result.wait_for(TIMEOUT) != std::future_status::ready) {
    std::cerr << "FAIL " << name << ": no response within timeout" << std::endl;
    std::_Exit(1);
  }
  if (auto const body = result.get(); !body.starts_with("HELLO WORLD")) {
    std::cerr << "FAIL " << name << ": unexpected body: [" << body << "]" << std::endl;
    std::_Exit(1);
  }
  std::cout << "PASS " << name << std::endl;
}

}  // namespace

void test_closed_stdin() {
  close_stdin_pipe();

  mkn::ram::http::MultiServer http(HTTP_PORT);
  http.withResponse(respond);
  http.start();

  mkn::ram::https::MultiServer https(HTTPS_PORT, 1, 1, mkn::kul::File("res/test/server.crt"),
                                     mkn::kul::File("res/test/server.key"));
  https.init().withResponse(respond);
  https.start();

  check<mkn::ram::http::Get>("http", HTTP_PORT);
  check<mkn::ram::https::Get>("https", HTTPS_PORT);

  http.stop();
  https.stop();
  http.join();
  https.join();
}

#else
void test_closed_stdin() {}
#endif  // _WIN32
