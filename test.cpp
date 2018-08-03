/**
Copyright (c) 2016, Philip Deegan.
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
#include "usage.cpp"
class TestHTTPSServer : public kul::https::Server {
private:
  void operator()() { start(); }

public:
  TestHTTPSServer()
      : kul::https::Server(_KUL_HTTP_TEST_PORT_,
                           kul::File("res/test/server.crt"),
                           kul::File("res/test/server.key")) {}
  friend class kul::Thread;
};
class HTTPS_Get : public kul::https::_1_1GetRequest {
public:
  HTTPS_Get(const std::string &host, const std::string &path = "",
            const uint16_t &port = 80)
      : kul::https::_1_1GetRequest(host, path, port) {}
};
int main(int argc, char *argv[]) {
  using namespace kul::http;
  {
    TestHTTPSServer serv;
    serv.init().withResponse([&](const A1_1Request &r) -> _1_1Response {
      KLOG(INF) << kul::os::EOL() << r.toString();
      _1_1Response rs;
      return rs.withBody("HELLO WORLD");
    });
    kul::Thread t(std::ref(serv));
    t.run();
    kul::this_thread::sleep(333);
    if (t.exception())
      std::rethrow_exception(t.exception());
    {
      HTTPS_Get get("localhost", "index.html", _KUL_HTTP_TEST_PORT_);
      KLOG(INF) << kul::os::EOL() << get.toString();
      get.withResponse([&](const kul::http::_1_1Response &r) {
           KLOG(INF) << kul::os::EOL() << r.toString();
         })
          .send();
    }
    serv.stop();
    t.join();
  }
  return 0;
}
