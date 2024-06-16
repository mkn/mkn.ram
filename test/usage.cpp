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

#ifdef _WIN32
#define bzero ZeroMemory
#endif

#ifndef _MKN_RAM_HTTP_TEST_PORT_
#define _MKN_RAM_HTTP_TEST_PORT_ 8888
#endif /*_MKN_RAM_HTTP_TEST_PORT_*/

namespace mkn {
namespace ram {

class TestHTTPServer : public mkn::ram::http::Server {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    mkn::ram::http::_1_1Response r;
    return r.withBody("HTTP PROVIDED BY KUL").withDefaultHeaders();
  }
  TestHTTPServer() : mkn::ram::http::Server(_MKN_RAM_HTTP_TEST_PORT_) {}
  friend class mkn::kul::Thread;
};

class TestMultiHTTPServer : public mkn::ram::http::MultiServer {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    mkn::ram::http::_1_1Response r;
    return r.withBody("MULTI HTTP PROVIDED BY KUL").withDefaultHeaders();
  }
  TestMultiHTTPServer() : mkn::ram::http::MultiServer(_MKN_RAM_HTTP_TEST_PORT_, 3) {}
  friend class mkn::kul::Thread;
};

#ifdef _MKN_RAM_INCLUDE_HTTPS_
class TestHTTPSServer : public mkn::ram::https::Server {
 private:
  void operator()() { start(); }

 public:
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const& req) {
    mkn::ram::http::_1_1Response r;
    return r.withBody("HTTPS PROVIDED BY KUL: " + req.method()).withDefaultHeaders();
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
    mkn::ram::http::_1_1Response r;
    return r.withBody("MULTI HTTPS PROVIDED BY KUL: " + req.method()).withDefaultHeaders();
  }
  TestMultiHTTPSServer(const uint8_t& acceptThreads = 1, const uint8_t& workerThreads = 1)
      : mkn::ram::https::MultiServer(_MKN_RAM_HTTP_TEST_PORT_, acceptThreads, workerThreads,
                                     mkn::kul::File("res/test/server.crt"),
                                     mkn::kul::File("res/test/server.key")) {}
  friend class mkn::kul::Thread;
};

class HTTPS_Get : public mkn::ram::https::_1_1GetRequest {
 public:
  HTTPS_Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1GetRequest(host, path, port) {}
  void handleResponse(mkn::ram::http::_1_1Response const& r) override {
    KLOG(INF) << "HTTPS GET RESPONSE: " << r.body();
  }
};
class HTTPS_Post : public mkn::ram::https::_1_1PostRequest {
 public:
  HTTPS_Post(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::https::_1_1PostRequest(host, path, port) {}
  void handleResponse(const mkn::ram::http::_1_1Response& r) override {
    for (const auto& p : r.headers()) KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
    KOUT(NON) << "HTTPS POST RESPONSE: " << r.body();
  }
};
#endif  //_MKN_RAM_INCLUDE_HTTPS_

class TestSocketServer : public mkn::ram::tcp::SocketServer<char> {
 private:
  void operator()() { start(); }

 public:
  bool handle(char* const in, size_t const& inLen, char* const out, size_t& outLen) override {
    std::string rep("TCP PROVIDED BY KUL");
    std::strcpy(out, rep.c_str());
    outLen = rep.size();
    return true;
  }
  TestSocketServer() : mkn::ram::tcp::SocketServer<char>(_MKN_RAM_HTTP_TEST_PORT_) {}
  friend class mkn::kul::Thread;
};

class Get : public mkn::ram::http::_1_1GetRequest {
 public:
  Get(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::http::_1_1GetRequest(host, path, port) {}
  void handleResponse(const mkn::ram::http::_1_1Response& r) override {
    KLOG(INF) << "GET RESPONSE: " << r.body();
  }
};
class Post : public mkn::ram::http::_1_1PostRequest {
 public:
  Post(std::string const& host, std::string const& path = "", uint16_t const& port = 80)
      : mkn::ram::http::_1_1PostRequest(host, path, port) {}
  void handleResponse(const mkn::ram::http::_1_1Response& r) override {
    for (const auto& p : r.headers()) KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
    KOUT(NON) << "HTTPS POST RESPONSE: " << r.body();
  }
};

class Test {
 public:
  Test() {
#ifdef _MKN_RAM_INCLUDE_HTTPS_
    KOUT(NON) << "Single HTTPS SERVER";
    {
      TestHTTPSServer serv;
      serv.init();
      mkn::kul::Thread t(std::ref(serv));
      t.run();
      mkn::kul::this_thread::sleep(333);
      if (t.exception()) std::rethrow_exception(t.exception());
      {
        HTTPS_Get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_).send();
        if (t.exception()) std::rethrow_exception(t.exception());
        HTTPS_Post p("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_);
        p.body("tsop");
        p.send();
        if (t.exception()) std::rethrow_exception(t.exception());
        HTTPS_Get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_).send();
        if (t.exception()) std::rethrow_exception(t.exception());
      }
      mkn::kul::this_thread::sleep(100);
      serv.stop();
      t.join();
      mkn::kul::this_thread::sleep(100);
    }
    KOUT(NON) << "Multi HTTPS SERVER";
    {
      TestMultiHTTPSServer serv(1, 5);
      serv.init();
      mkn::kul::Thread t(std::ref(serv));
      t.run();
      mkn::kul::this_thread::sleep(333);

      std::atomic<uint16_t> index(0);
      auto getter = [&]() {
        HTTPS_Get("localhost", "index.html_" + std::to_string(index++), _MKN_RAM_HTTP_TEST_PORT_)
            .send();
      };
      auto except = [&t](const mkn::kul::Exception& e) {
        KLOG(ERR) << e.stack();
        if (t.exception()) std::rethrow_exception(t.exception());
      };
      mkn::kul::ChroncurrentThreadPool<> ctp(3, 1);
      for (size_t i = 0; i < 999; i++) ctp.async(getter, except);
      ctp.finish(1000000000);

      mkn::kul::this_thread::sleep(100);
      serv.stop();
      mkn::kul::this_thread::sleep(100);
      serv.join();
      t.join();
    }

#endif  // _MKN_RAM_HTTPS_

    KOUT(NON) << "Single HTTP SERVER";
    {
      TestHTTPServer serv;
      mkn::kul::Thread t(std::ref(serv));
      t.run();
      mkn::kul::this_thread::sleep(333);
      if (t.exception()) std::rethrow_exception(t.exception());
      {
        Get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_).send();
        Get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_).send();
        if (t.exception()) std::rethrow_exception(t.exception());
        Post p("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_);
        p.body("tsop");
        p.send();
        if (t.exception()) std::rethrow_exception(t.exception());
      }
      mkn::kul::this_thread::sleep(100);
      serv.stop();
      mkn::kul::this_thread::sleep(100);
      t.join();
    }
    KLOG(INF) << "Test socket connection";
    {
      mkn::ram::tcp::Socket<char> sock;
      if (!sock.connect("google.com", 80))
        KEXCEPT(mkn::ram::tcp::Exception, "TCP FAILED TO CONNECT!");

      Get get("google.com");
      std::string req(get.toString());
      std::vector<char> v1, v2;
      KOUT(NON) << "Writing to TCP socket";
      for (size_t i = 0; i < req.size() / 2; i++) v1.push_back(req.at(i));
      for (size_t i = req.size() / 2; i < req.size(); i++) v2.push_back(req.at(i));
      std::string s1(v1.begin(), v1.end()), s2(v2.begin(), v2.end());
      sock.write(s1.c_str(), s1.size());
      sock.write(s2.c_str(), s2.size());
      KOUT(NON) << "Reading from TCP socket";
      char buf[_MKN_RAM_TCP_REQUEST_BUFFER_];
      bzero(buf, _MKN_RAM_TCP_REQUEST_BUFFER_);
      sock.read(buf, _MKN_RAM_TCP_REQUEST_BUFFER_);
      sock.close();
    }
    KOUT(NON) << "TCP Socket SERVER";
    mkn::kul::this_thread::sleep(500);
    {
      TestSocketServer serv;
      mkn::kul::Thread t(std::ref(serv));
      t.run();
      mkn::kul::this_thread::sleep(333);
      if (t.exception()) std::rethrow_exception(t.exception());

      for (size_t i = 0; i < 5; i++) {
        mkn::ram::tcp::Socket<char> sock;
        if (!sock.connect("localhost", _MKN_RAM_HTTP_TEST_PORT_))
          KEXCEPT(mkn::ram::tcp::Exception, "TCP FAILED TO CONNECT!");

        const char* c = "socketserver";
        sock.write(c, strlen(c));

        char buf[_MKN_RAM_TCP_REQUEST_BUFFER_];
        bzero(buf, _MKN_RAM_TCP_REQUEST_BUFFER_);
        sock.read(buf, _MKN_RAM_TCP_REQUEST_BUFFER_);

        sock.close();
      }

      serv.stop();
      t.join();
    }
    //     mkn::kul::this_thread::sleep(100);
    //     {
    //         TestMultiHTTPServer serv;
    //         mkn::kul::Thread t(std::ref(serv));
    //         t.run();
    //         mkn::kul::this_thread::sleep(333);
    //         for(size_t i = 0; i < 10; i++){
    //             Get("localhost", "index.html", _MKN_RAM_HTTP_TEST_PORT_).send();
    //             if(t.exception()) std::rethrow_exception(t.exception());
    //         }
    //         mkn::kul::this_thread::sleep(100);
    //         serv.stop();
    //         mkn::kul::this_thread::sleep(100);
    //         serv.join();
    //     }
    //     KOUT(NON) << "FINISHED!";
  }
};
}  // namespace ram
}  // namespace mkn

#ifndef __MKN_RAM_NOMAIN__
int main(int argc, char* argv[]) {
  mkn::kul::Signal s;
  try {
    mkn::ram::Test();

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
#endif  //__MKN_RAM_NOMAIN__