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
//
// MAY REQUIRE: runas admin - netsh http add urlacl url=http://localhost:666/
// user=EVERYONE listen=yes delegate=no
//
#ifndef _MKN_RAM_OS_WIN_HTTP_HPP_
#define _MKN_RAM_OS_WIN_HTTP_HPP_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "mkn/kul/log.hpp"
#include "mkn/kul/threads.hpp"

#ifndef UNICODE
#define UNICODE
#endif

#ifndef MAX_ULONG_STR
#define MAX_ULONG_STR ((ULONG)sizeof("4294967295"))
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <unordered_map>

#pragma comment(lib, "httpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace mkn {
namespace ram {
namespace http {

class Server : public mkn::ram::http::AServer {
 private:
  int fdSize = _MKN_RAM_TCP_READ_BUFFER_;
  std::unordered_map<int, std::unique_ptr<char[]>> inBuffers;

 protected:
  virtual char* getOrCreateBufferFor(const int& fd) {
    if (!inBuffers.count(fd))
      inBuffers.insert(std::make_pair(fd, std::unique_ptr<char[]>(new char[fdSize])));
    return inBuffers[fd].get();
  }

  virtual KUL_PUBLISH bool receive(std::map<int, uint8_t>& fds, const int& fd) override;

 public:
  Server(const short& p = 80) : AServer(p) {}
  virtual ~Server() {}
};

class KUL_PUBLISH MultiServer : public mkn::ram::http::Server {
 protected:
  uint8_t _acceptThreads, _workerThreads;
  mkn::kul::Mutex m_mutex;
  mkn::kul::ChroncurrentThreadPool<> _acceptPool;
  mkn::kul::ChroncurrentThreadPool<> _workerPool;

  virtual void handleBuffer(std::map<int, uint8_t>& fds, const int& fd, char* in, const int& read,
                            int& e) override {
    _workerPool.async(
        std::bind(&MultiServer::operateBuffer, std::ref(*this), &fds, fd, in, read, e),
        std::bind(&MultiServer::errorBuffer, std::ref(*this), std::placeholders::_1));
    e = 1;
  }

  void operateBuffer(std::map<int, uint8_t>* fds, const int& fd, char* in, const int& read,
                     int& e) {
    mkn::ram::http::Server::handleBuffer(*fds, fd, in, read, e);
    if (e < 0) {
      std::vector<int> del{fd};
      // closeFDs(*fds, del);
    }
  }
  virtual void errorBuffer(const mkn::kul::Exception& e) { KERR << e.stack(); };

  void operateAccept(const size_t& threadID) {
    std::map<int, uint8_t> fds;
    fds.insert(std::make_pair(0, 0));
    for (int i = threadID; i < _MKN_RAM_TCP_MAX_CLIENT_; i += _acceptThreads)
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

 public:
  MultiServer(const short& p = 80, const uint8_t& acceptThreads = 1,
              const uint8_t& workerThreads = 1)
      : Server(p), _acceptThreads(acceptThreads), _workerThreads(workerThreads) {}

  virtual void start() KTHROW(mkn::ram::tcp::Exception) override;

  virtual void join() {
    _acceptPool.join();
    _workerPool.join();
  }
  virtual void stop() override {
    mkn::ram::http::Server::stop();
    _acceptPool.stop();
    _workerPool.stop();
  }
  virtual void interrupt() {
    _acceptPool.interrupt();
    _workerPool.interrupt();
  }
  const std::exception_ptr& exception() { return _acceptPool.exception(); }
};

}  // namespace http
}  // namespace ram
}  // namespace mkn

#endif /* _MKN_RAM_OS_WIN_HTTP_HPP_ */
