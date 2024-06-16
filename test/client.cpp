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
#include <cstring>

#include "mkn/kul/signal.hpp"
#include "mkn/ram/http.hpp"
#include "mkn/ram/tcp.hpp"

#ifdef _MKN_RAM_INCLUDE_HTTPS_
#include "mkn/ram/https.hpp"
#endif  //_MKN_RAM_INCLUDE_HTTPS_

#include "mkn/ram/html4.hpp"

#ifndef _MKN_RAM_HTTP_TEST_PORT_
#define _MKN_RAM_HTTP_TEST_PORT_ 8888
#endif /*_MKN_RAM_HTTP_TEST_PORT_*/
#ifdef _MKN_RAM_INCLUDE_HTTPS_

namespace mkn {
namespace ram {

class HTTPS_Get : public mkn::ram::https::_1_1GetRequest {
 public:
  HTTPS_Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1GetRequest(host, path, port) {}
  void handleResponse(const mkn::kul::hash::map::S2S& h, std::string const& b) override {
    KLOG(INF) << "HTTPS GET RESPONSE:\n" << b;
  }
};
class HTTPS_Post : public mkn::ram::https::_1_1PostRequest {
 public:
  HTTPS_Post(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1PostRequest(host, path, port) {}
  void handleResponse(const mkn::kul::hash::map::S2S& h, std::string const& b) override {
    KUL_DBG_FUNC_ENTER
    for (const auto& p : h) KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
    KOUT(NON) << "HTTPS POST RESPONSE:\n" << b;
  }
};
#endif  //_MKN_RAM_INCLUDE_HTTPS_

class Get : public mkn::ram::http::_1_1GetRequest {
 public:
  Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::http::_1_1GetRequest(host, path, port) {}
  void handleResponse(const mkn::kul::hash::map::S2S& h, std::string const& b) override {
    KLOG(INF) << b;
    // if(b.substr(0, b.size() - 2) != "MULTI HTTP PROVIDED BY KUL")
    // KEXCEPTION("Body failed :" + b + ":");
  }
};
class Post : public mkn::ram::http::_1_1PostRequest {
 public:
  Post(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::http::_1_1PostRequest(host, path, port) {}
  void handleResponse(const mkn::kul::hash::map::S2S& h, std::string const& b) override {
    if (b.substr(0, b.size() - 2) != "MULTI HTTP PROVIDED BY KUL")
      KEXCEPTION("Body failed :" + b + ":");
  }
};
}
}

int main(int argc, char* argv[]) {
  mkn::kul::Signal s;
  try {
    mkn::kul::ram::Get g("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_);
    mkn::kul::ChroncurrentThreadPool<> requests(10, 0);

    for (size_t i = 0; i < 5000; i++) requests.async([&]() { g.send(); });
    requests.start().finish();

    // mkn::kul::ram::Get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_).send();
    // for(size_t i = 0; i < 100; i++)
    //     mkn::kul::ram::Post("localhost", "index.html",
    //     _MKN_RAM_HTTP_TEST_PORT_).send();

    // mkn::kul::this_thread::sleep(500);

    // for(size_t i = 0; i < 1000; i++)
    //     mkn::kul::ram::Get("localhost", "index.html",
    //     _MKN_RAM_HTTP_TEST_PORT_).send();
    // for(size_t i = 0; i < 1000; i++)
    //     mkn::kul::ram::Post("localhost", "index.html",
    //     _MKN_RAM_HTTP_TEST_PORT_).send();

    // mkn::kul::this_thread::sleep(500);

    // #ifdef  _MKN_RAM_INCLUDE_HTTPS_
    //         for(size_t i = 0; i < 100; i++)
    //             mkn::kul::ram::HTTPS_Get("localhost", "index.html",
    //             _MKN_RAM_HTTP_TEST_PORT_).send();
    //         for(size_t i = 0; i < 100; i++)
    //             mkn::kul::ram::HTTPS_Post("localhost", "index.html",
    //             _MKN_RAM_HTTP_TEST_PORT_).send();

    //         mkn::kul::this_thread::sleep(500);

    //         for(size_t i = 0; i < 1000; i++)
    //             mkn::kul::ram::HTTPS_Get("localhost", "index.html",
    //             _MKN_RAM_HTTP_TEST_PORT_).send();
    //         for(size_t i = 0; i < 1000; i++)
    //             mkn::kul::ram::HTTPS_Post("localhost", "index.html",
    //             _MKN_RAM_HTTP_TEST_PORT_).send();
    // #endif//_MKN_RAM_INCLUDE_HTTPS_

  } catch (const mkn::kul::Exception& e) {
    KERR << e.stack();
    return 1;
  } catch (const std::exception& e) {
    KERR << e.what();
    return 2;
  } catch (...) {
    KERR << "UNKNOWN EXCEPTION CAUGHT";
    return 3;
  }
  return 0;
}
