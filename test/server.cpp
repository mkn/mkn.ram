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

#include "mkn/kul/tcp.hpp"
#include "mkn/ram/http.hpp"

#ifdef _MKN_RAM_INCLUDE_HTTPS_
#include "mkn/ram/https.hpp"
#endif  //_MKN_RAM_INCLUDE_HTTPS_

#include "mkn/kul/html4.hpp"
#include "mkn/kul/signal.hpp"

#ifndef _MKN_RAM_HTTP_TEST_PORT_
#define _MKN_RAM_HTTP_TEST_PORT_ 8888
#endif /*_MKN_RAM_HTTP_TEST_PORT_*/

namespace mkn {
namespace ram {

void addResponseHeaders(mkn::ram::http::_1_1Response& r) {
  if (!r.header("Date")) r.header("Date", mkn::kul::DateTime::NOW());
  if (!r.header("Connection")) r.header("Connection", "close");
  if (!r.header("Content-Type")) r.header("Content-Type", "text/html");
  if (!r.header("Content-Length")) r.header("Content-Length", std::to_string(r.body().size()));
}

class TestHTTPServer : public mkn::ram::http::Server {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    KUL_DBG_FUNC_ENTER
    mkn::ram::http::_1_1Response r;
    r.body("HTTP PROVIDED BY KUL");
    addResponseHeaders(r);
    return r;
  }
  TestHTTPServer() : mkn::ram::http::Server(_MKN_RAM_HTTP_TEST_PORT_) {}
  friend class mkn::kul::Thread;
};

class TestMultiHTTPServer : public mkn::ram::http::MultiServer {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    KUL_DBG_FUNC_ENTER
    mkn::ram::http::_1_1Response r;
    r.body("MULTI HTTP PROVIDED BY KUL");
    addResponseHeaders(r);
    return r;
  }
  TestMultiHTTPServer(uint16_t const& threads)
      : mkn::ram::http::MultiServer(_MKN_RAM_HTTP_TEST_PORT_, threads) {}
  TestMultiHTTPServer() : mkn::ram::http::MultiServer(_MKN_RAM_HTTP_TEST_PORT_, 2, 4) {}
  friend class mkn::kul::Thread;
};

#ifdef _MKN_RAM_INCLUDE_HTTPS_
class TestHTTPSServer : public mkn::ram::https::Server {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    KUL_DBG_FUNC_ENTER
    mkn::ram::http::_1_1Response r;
    r.body("HTTPS PROVIDED BY KUL: " + req.method());
    addResponseHeaders(r);
    return r;
  }
  TestHTTPSServer()
      : mkn::ram::https::Server(_MKN_RAM_HTTP_TEST_PORT_, mkn::kul::File("res/test/server.crt"),
                                mkn::kul::File("res/test/server.key")) {}
  friend class mkn::kul::Thread;
};

class TestMultiHTTPSServer : public mkn::ram::https::MultiServer {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    KUL_DBG_FUNC_ENTER
    mkn::ram::http::_1_1Response r;
    r.body("MULTI HTTPS PROVIDED BY KUL");
    addResponseHeaders(r);
    return r;
  }
  TestMultiHTTPSServer()
      : mkn::ram::https::MultiServer(_MKN_RAM_HTTP_TEST_PORT_, 3,
                                     mkn::kul::File("res/test/server.crt"),
                                     mkn::kul::File("res/test/server.key")) {}
  friend class mkn::kul::Thread;
};

class HTTPS_Get : public mkn::ram::https::_1_1GetRequest {
 public:
  HTTPS_Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1GetRequest(host, path, port) {}
  void handleResponse(mkn::kul::hash::map::S2S const& h, std::string const& b) override {
    KLOG(INF) << "HTTPS GET RESPONSE:\n" << b;
  }
};
class HTTPS_Post : public mkn::ram::https::_1_1PostRequest {
 public:
  HTTPS_Post(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1PostRequest(host, path, port) {}
  void handleResponse(mkn::kul::hash::map::S2S const& h, std::string const& b) override {
    KUL_DBG_FUNC_ENTER
    for (auto const& p : h) KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
    KOUT(NON) << "HTTPS POST RESPONSE:\n" << b;
  }
};
#endif  //_MKN_RAM_INCLUDE_HTTPS_

class TestSocketServer : public mkn::ram::tcp::SocketServer<char> {
 private:
  void operator()() { start(); }

 public:
  bool handle(char* in, char* out) override {
    KUL_DBG_FUNC_ENTER
    mkn::ram::http::_1_1Response getResponse;
    addResponseHeaders(getResponse);
    getResponse.body("magicmansion");
    std::string getResponseStr(getResponse.toString());
    std::strcpy(out, getResponseStr.c_str());
    return true;  // if true, close connection
  }
  TestSocketServer() : mkn::ram::tcp::SocketServer<char>(_MKN_RAM_HTTP_TEST_PORT_) {}
  friend class mkn::kul::Thread;
};

class Get : public mkn::ram::http::_1_1GetRequest {
 public:
  Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::http::_1_1GetRequest(host, path, port) {}
  void handleResponse(mkn::kul::hash::map::S2S const& h, std::string const& b) override {
    KLOG(INF) << "GET RESPONSE:\n" << b;
  }
};
class Post : public mkn::ram::http::_1_1PostRequest {
 public:
  Post(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::http::_1_1PostRequest(host, path, port) {}
  void handleResponse(mkn::kul::hash::map::S2S const& h, std::string const& b) override {
    KUL_DBG_FUNC_ENTER
    for (auto const& p : h) KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
    KOUT(NON) << "POST RESPONSE:\n" << b;
  }
};
}  // namespace ram
}  // namespace mkn

int main(int argc, char* argv[]) {
  mkn::kul::Signal s;
  try {
    // KOUT(NON) << "TCP Socket SERVER";
    // {
    //     mkn::kul::ram::TestSocketServer serv;
    //     mkn::kul::Thread t(std::ref(serv));
    //     t.run();
    //     mkn::kul::this_thread::sleep(100);
    //     if(t.exception()) std::rethrow_exception(t.exception());
    //     s.abrt([&](int16_t i){ serv.stop(); });
    //     t.join();
    //     serv.stop();
    // }
    // KOUT(NON) << "Single HTTP SERVER";
    // {
    //     mkn::kul::ram::TestHTTPServer serv;
    //     mkn::kul::Thread t(std::ref(serv));
    //     t.run();
    //     mkn::kul::this_thread::sleep(100);
    //     if(t.exception()) std::rethrow_exception(t.exception());
    //     s.abrt([&](int16_t i){ serv.stop(); });
    //     t.join();
    // }
    KOUT(NON) << "Multi HTTP SERVER";
    {
      mkn::kul::ram::TestMultiHTTPServer serv(10);
      mkn::kul::Thread t(std::ref(serv));

      t.run();
      s.abrt([&](int16_t i) { serv.stop(); });

      mkn::kul::this_thread::sleep(100);
      if (t.exception()) std::rethrow_exception(t.exception());
      mkn::kul::this_thread::sleep(100);
      serv.join();
      serv.stop();
      mkn::kul::this_thread::sleep(100);
    }
#ifdef _MKN_RAM_INCLUDE_HTTPS_
// KOUT(NON) << "Single HTTPS SERVER";
// {
//     mkn::kul::ram::TestHTTPSServer serv;
//     serv.init();
//     mkn::kul::Thread t(std::ref(serv));
//     t.run();
//     mkn::kul::this_thread::sleep(100);
//     if(t.exception()) std::rethrow_exception(t.exception());
//     mkn::kul::this_thread::sleep(1000);
//     serv.stop();
//     mkn::kul::this_thread::sleep(100);
//     t.join();
// }
// KOUT(NON) << "Multi HTTPS SERVER";
// {
//     mkn::kul::ram::TestMultiHTTPSServer serv;
//     serv.init();
//     mkn::kul::Thread t(std::ref(serv));
//     t.run();
//     mkn::kul::this_thread::sleep(100);
//     if(t.exception()) std::rethrow_exception(t.exception());
//     mkn::kul::this_thread::sleep(5000);
//     serv.join();
//     serv.stop();
//     mkn::kul::this_thread::sleep(100);
//     t.join();
// }
#endif  //_MKN_RAM_INCLUDE_HTTPS_

  } catch (const mkn::kul::Exception& e) {
    KERR << e.stack();
    return 1;
  } catch (std::exception const& e) {
    KERR << e.what();
    return 2;
  } catch (...) {
    KERR << "UNKNOWN EXCEPTION CAUGHT";
    return 3;
  }
  return 0;
}
