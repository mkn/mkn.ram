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
#ifndef _KUL_ASIO_FCGI_HPP_
#define _KUL_ASIO_FCGI_HPP_

#include <cstring>

#include "kul/fcgi/def.hpp"
#include "kul/tcp.hpp"
#include "kul/threads.hpp"

namespace kul {
namespace asio {
namespace fcgi {

class Exception : public kul::Exception {
public:
  Exception(const char *f, const uint16_t &l, const std::string &s)
      : kul::Exception(f, l, s) {}
};

class Server;
class FCGI_Message {
  friend class Server;

private:
  bool d = 1;
  uint16_t rid = 0;
  std::function<void(const FCGI_Message &msg)> m_finish;

private:
  FCGI_Message(const FCGI_Message &msg) = delete;
  FCGI_Message &operator=(const FCGI_Message &msg) = delete;

  void finish() { m_finish(*this); }
  void finish(const std::function<void(const FCGI_Message &msg)> &fin) {
    m_finish = fin;
  }

protected:
  virtual std::string response() const { return "FCGI-ed"; }
  bool done() {
    if (d)
      return true;
    d = !d;
    return false;
  }

public:
  FCGI_Message() {}
};

class Server : public kul::tcp::SocketServer<uint8_t> {
protected:
  int fdSize = _KUL_TCP_READ_BUFFER_;
  uint8_t m_acceptThreads, m_workerThreads;

  kul::Mutex m_actex, m_mutex, m_butex, m_mapex;
  kul::ChroncurrentThreadPool<> m_acceptPool;
  kul::ChroncurrentThreadPool<> m_workerPool;

  std::unordered_map<uint64_t, std::unique_ptr<FCGI_Message>> msgs;
  std::unordered_map<int, std::unique_ptr<uint8_t[]>> inBuffers;

protected:
  uint8_t *getOrCreateBufferFor(const int &fd) {
    kul::ScopeLock lock(m_butex);
    if (!inBuffers.count(fd))
      inBuffers.insert(
          std::make_pair(fd, std::unique_ptr<uint8_t[]>(new uint8_t[fdSize])));
    return inBuffers[fd].get();
  }

  FCGI_Message &createFCGI_Message(const int &fd) {
    kul::ScopeLock lock(m_mapex);
    msgs[fd] = std::make_unique<FCGI_Message>();
    return *msgs[fd];
  }

  int accept(const uint16_t &fd) override {
    kul::ScopeLock lock(m_actex);
    return ::accept(lisock, (struct sockaddr *)&cli_addr[fd], &clilen);
  }

  void cycle(const uint16_t &size, std::map<int, uint8_t> *fds, const int &fd) {
    auto &msg(*msgs[fd]);
    work(msg);
    if (msg.done()) {
      uint8_t *out = getOrCreateBufferFor(fd);
      size_t size = FORM_RESPONSE(msg, out);
      write(*fds, fd, out, size);
    }
  }

  void write(std::map<int, uint8_t> &fds, const int &fd, const uint8_t *out,
             const size_t size);

  bool receive(std::map<int, uint8_t> &fds, const int &fd) override;

  void PARAMS(uint8_t *const in, const int &inLen, FCGI_Message &msg) {}

  void PARSE_FIRST(std::map<int, uint8_t> &fds, uint8_t *const in,
                   const int &inLen, const int &fd)
      KTHROW(kul::fcgi::Exception);

  void PARSE(std::map<int, uint8_t> &fds, uint8_t *const in, const int &inLen,
             const int &fd, size_t pos) KTHROW(kul::fcgi::Exception);

  size_t FORM_RESPONSE(const FCGI_Message &msg, uint8_t *out);

  virtual void work(FCGI_Message &msg) {}

  void operateAccept(const size_t &threadID) {
    std::map<int, uint8_t> fds;
    fds.insert(std::make_pair(0, 0));
    for (size_t i = threadID; i < _KUL_TCP_MAX_CLIENT_; i += m_acceptThreads)
      fds.insert(std::make_pair(i, 0));
    while (s)
      try {
        kul::ScopeLock lock(m_mutex);
        loop(fds);
      } catch (const kul::tcp::Exception &e1) {
        KERR << e1.stack();
      } catch (const std::exception &e1) {
        KERR << e1.what();
      } catch (...) {
        KERR << "Loop Exception caught";
      }
  }
  void handleBuffer(std::map<int, uint8_t> &fds, const int &fd, uint8_t *in,
                    const int &read, int &e) {
    m_workerPool.async(std::bind(&Server::operateBuffer, std::ref(*this), &fds,
                                 fd, in, read, e),
                       std::bind(&Server::errorBuffer, std::ref(*this),
                                 std::placeholders::_1));
  }

  void operateBuffer(std::map<int, uint8_t> *fds, const int &fd, uint8_t *in,
                     const int &read, int &e) {
    PARSE_FIRST(*fds, in, read, fd);
    if (e < 0) {
      std::vector<int> del{fd};
      closeFDs(*fds, del);
    }
  }
  void errorBuffer(const kul::Exception &e) { KERR << e.stack(); };

public:
  Server(const uint16_t &port, const uint8_t &acceptThreads = 1,
         const uint8_t &workerThreads = 1)
      : kul::tcp::SocketServer<uint8_t>(port), m_acceptThreads(acceptThreads),
        m_workerThreads(workerThreads), m_acceptPool(acceptThreads),
        m_workerPool(workerThreads) {
    if (acceptThreads < 1)
      KEXCEPTION("FCGI Server cannot have less than one threads for accepting");
    if (workerThreads < 1)
      KEXCEPTION("FCGI Server cannot have less than one threads for working");
  }

  virtual ~Server() {
    m_acceptPool.stop();
    m_workerPool.stop();
  }

  void start() KTHROW(kul::tcp::Exception) override;

  void join() {
    if (!started())
      start();
    m_acceptPool.join();
    m_workerPool.join();
  }
  void stop() override {
    kul::tcp::SocketServer<uint8_t>::stop();
    m_acceptPool.stop();
    m_workerPool.stop();
  }
  void interrupt() {
    m_acceptPool.interrupt();
    m_workerPool.interrupt();
  }
};

} // namespace fcgi
} // namespace asio
} // END NAMESPACE kul

#endif /* _KUL_ASIO_FCGI_HPP_ */
