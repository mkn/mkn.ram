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
#ifndef _MKN_RAM_HTTPS_HPP_
#define _MKN_RAM_HTTPS_HPP_

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "mkn/ram/http.hpp"

#define MKN_RAM_HTTPS_METHOD_APPENDER2(x, y) x##y
#define MKN_RAM_HTTPS_METHOD_APPENDER(x, y) MKN_RAM_HTTPS_METHOD_APPENDER2(x, y)

#if !defined(_MKN_RAM_HTTPS_CLIENT_METHOD_) && !defined(_MKN_RAM_HTTPS_SERVER_METHOD_)

#ifndef _MKN_RAM_HTTPS_METHOD_
#define _MKN_RAM_HTTPS_METHOD_ TLS
#endif /* _MKN_RAM_HTTPS_METHOD_ */

#define _MKN_RAM_HTTPS_CLIENT_METHOD_ \
  MKN_RAM_HTTPS_METHOD_APPENDER(_MKN_RAM_HTTPS_METHOD_, _client_method)
#define _MKN_RAM_HTTPS_SERVER_METHOD_ \
  MKN_RAM_HTTPS_METHOD_APPENDER(_MKN_RAM_HTTPS_METHOD_, _server_method)

#else

#ifndef _MKN_RAM_HTTPS_CLIENT_METHOD_
#define _MKN_RAM_HTTPS_CLIENT_METHOD_ TLS_client_method
#endif /* _MKN_RAM_HTTPS_CLIENT_METHOD_ */
#ifndef _MKN_RAM_HTTPS_SERVER_METHOD_
#define _MKN_RAM_HTTPS_SERVER_METHOD_ TLS_server_method
#endif /* _MKN_RAM_HTTPS_SERVER_METHOD_ */

#endif /* defined xyz */

namespace mkn {
namespace ram {
namespace https {

class Server : public mkn::ram::http::Server {
 protected:
  X509* cc = {0};
  SSL* ssl_clients[_MKN_RAM_TCP_MAX_CLIENT_] = {0};
  SSL_CTX* ctx = {0};
  mkn::kul::File crt, key;
  const std::string cs;

  virtual KUL_PUBLISH void loop(std::map<int, uint8_t>& fds) KTHROW(kul::tcp::Exception) override;

  virtual KUL_PUBLISH bool receive(std::map<int, uint8_t>& fds, const int& fd) override;

  virtual KUL_PUBLISH void handleBuffer(std::map<int, uint8_t>& fds, const int& fd, char* in,
                                        const int& read, int& e);

 public:
  Server(const short& p, const mkn::kul::File& c, const mkn::kul::File& k,
         const std::string& cs = "")
      : mkn::ram::http::Server(p), crt(c), key(k), cs(cs) {}
  Server(const mkn::kul::File& c, const mkn::kul::File& k, const std::string& cs = "")
      : mkn::ram::https::Server(443, c, k, cs) {}
  virtual ~Server() {
    if (s) stop();
  }
  KUL_PUBLISH void setChain(const mkn::kul::File& f);
  KUL_PUBLISH Server& init();
  KUL_PUBLISH virtual void stop() override;
};

class KUL_PUBLISH MultiServer : public mkn::ram::https::Server {
 protected:
  uint8_t _acceptThreads, _workerThreads;
  mkn::kul::Mutex m_mutex;
  ChroncurrentThreadPool<> _acceptPool;
  ChroncurrentThreadPool<> _workerPool;

  void operateAccept(const size_t& threadID) {
    std::map<int, uint8_t> fds;
    fds.insert(std::make_pair(0, 0));
    for (size_t i = threadID; i < _MKN_RAM_TCP_MAX_CLIENT_; i += _acceptThreads)
      fds.insert(std::make_pair(i, 0));
    while (s) try {
        mkn::kul::ScopeLock lock(m_mutex);
        loop(fds);
      } catch (const mkn::ram::tcp::Exception& e1) {
        KERR << e1.stack();
      } catch (const std::exception& e1) {
        KERR << e1.what();
      } catch (...) {
        KERR << "Loop Exception caught";
      }
  }

  virtual void handleBuffer(std::map<int, uint8_t>& fds, const int& fd, char* in, const int& read,
                            int& e) override {
    _workerPool.async(
        std::bind(&MultiServer::operateBuffer, std::ref(*this), &fds, fd, in, read, e),
        std::bind(&MultiServer::errorBuffer, std::ref(*this), std::placeholders::_1));
    e = 1;
  }

  void operateBuffer(std::map<int, uint8_t>* fds, const int& fd, char* in, const int& read,
                     int& e) {
    mkn::ram::https::Server::handleBuffer(*fds, fd, in, read, e);
    if (e < 0) {
      std::vector<int> del{fd};
      closeFDs(*fds, del);
    }
  }
  virtual void errorBuffer(const mkn::kul::Exception& e) { KERR << e.stack(); };

 public:
  MultiServer(const short& p, const uint8_t& acceptThreads, const uint8_t& workerThreads,
              const mkn::kul::File& c, const mkn::kul::File& k, const std::string& cs = "")
      : mkn::ram::https::Server(p, c, k, cs),
        _acceptThreads(acceptThreads),
        _workerThreads(workerThreads),
        _acceptPool(acceptThreads),
        _workerPool(workerThreads) {
    if (acceptThreads < 1)
      KEXCEPTION("MultiServer cannot have less than one threads for accepting");
    if (workerThreads < 1) KEXCEPTION("MultiServer cannot have less than one threads for working");
  }
  MultiServer(const uint8_t& acceptThreads, const uint8_t& workerThreads, const mkn::kul::File& c,
              const mkn::kul::File& k, const std::string& cs = "")
      : MultiServer(443, acceptThreads, workerThreads, c, k, cs) {}

  virtual ~MultiServer() {
    _acceptPool.stop();
    _workerPool.stop();
  }

  virtual void start() KTHROW(kul::tcp::Exception) override;

  virtual void join() {
    _acceptPool.join();
    _workerPool.join();
  }
  virtual void stop() override {
    mkn::ram::https::Server::stop();
    _acceptPool.stop();
    _workerPool.stop();
  }
  virtual void interrupt() {
    _acceptPool.interrupt();
    _workerPool.interrupt();
  }
  const std::exception_ptr& exception() { return _acceptPool.exception(); }
};

class A1_1Request;
class Requester;
class SSLReqHelper {
  friend class A1_1Request;
  friend class Requester;

 private:
  SSL_CTX* ctx;
  SSLReqHelper() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    ctx = SSL_CTX_new(_MKN_RAM_HTTPS_CLIENT_METHOD_());
    if (ctx == NULL) {
      ERR_print_errors_fp(stderr);
      abort();
      KEXCEPTION("HTTPS Request SSL_CTX FAILED");
    }
  }
  ~SSLReqHelper() { SSL_CTX_free(ctx); }
  static SSLReqHelper& INSTANCE() {
    static SSLReqHelper i;
    return i;
  }
};

class A1_1Request {
 protected:
  SSL* ssl = {0};
  A1_1Request() : ssl(SSL_new(SSLReqHelper::INSTANCE().ctx)) {}
  ~A1_1Request() { SSL_free(ssl); }
};

class Requester {
 public:
  static void send(const std::string& h, const std::string& req, const uint16_t& p,
                   std::stringstream& ss, SSL* ssl);
};

class _1_1GetRequest : public http::_1_1GetRequest, https::A1_1Request {
 public:
  _1_1GetRequest(const std::string& host, const std::string& path = "", const uint16_t& port = 443)
      : http::_1_1GetRequest(host, path, port) {}
  virtual ~_1_1GetRequest() {}
  virtual void send() KTHROW(mkn::ram::http::Exception) override;
};
using Get = _1_1GetRequest;

class _1_1PostRequest : public http::_1_1PostRequest, https::A1_1Request {
 public:
  _1_1PostRequest(const std::string& host, const std::string& path = "", const uint16_t& port = 443)
      : http::_1_1PostRequest(host, path, port) {}
  virtual void send() KTHROW(mkn::ram::http::Exception) override;
};
using Post = _1_1PostRequest;

}  // namespace https
}  // namespace ram
}  // namespace mkn
#endif  //_MKN_RAM_INCLUDE_HTTPS_HPP_
