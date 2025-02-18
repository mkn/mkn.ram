/**
Copyright (c) 2024, Philip Deegan.
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
// #define _MKN_RAM_INCLUDE_HTTPS_
#define __MKN_RAM_NOMAIN__
#include "usage.cpp"
class TestHTTPSServer : public mkn::ram::https::Server {
 private:
  void operator()() { start(); }

 public:
  TestHTTPSServer()
      : mkn::ram::https::Server(_MKN_RAM_HTTP_TEST_PORT_, mkn::kul::File("res/test/server.crt"),
                                mkn::kul::File("res/test/server.key")) {}
  friend class mkn::kul::Thread;
};
class HTTPS_Get : public mkn::ram::https::_1_1GetRequest {
 public:
  HTTPS_Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1GetRequest(host, path, port) {}
};
int main(int argc, char* argv[]) {
  using namespace mkn::ram::http;
  {
    TestHTTPSServer serv;
    serv.init().withResponse([](A1_1Request const& r) {
      KLOG(NON) << mkn::kul::os::EOL() << r.toString();
      return _1_1Response{}.withBody("HELLO WORLD");
    });
    mkn::kul::Thread t(std::ref(serv));
    t.run();
    mkn::kul::this_thread::sleep(333);
    if (t.exception()) std::rethrow_exception(t.exception());
    {
      HTTPS_Get get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_);
      KLOG(NON) << mkn::kul::os::EOL() << get.toString();
      get.withResponse([](mkn::ram::http::_1_1Response const& r) {
           KLOG(INF) << mkn::kul::os::EOL() << r.toString();
         })
          .send();
    }
    serv.stop();
    t.join();
  }
  return 0;
}
