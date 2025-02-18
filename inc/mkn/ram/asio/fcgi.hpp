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
#ifndef _MKN_RAM_ASIO_FCGI_HPP_
#define _MKN_RAM_ASIO_FCGI_HPP_

#include <cstring>

#include "mkn/kul/threads.hpp"
#include "mkn/ram/fcgi/def.hpp"
#include "mkn/ram/tcp.hpp"

namespace mkn {
namespace ram {
namespace asio {
namespace fcgi {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

class Server;
class FCGI_Message {
  friend class Server;

 private:
  bool d = 1;
  uint16_t rid = 0;
  std::function<void(FCGI_Message const& msg)> m_finish;

 private:
  FCGI_Message(FCGI_Message const& msg) = delete;
  FCGI_Message& operator=(FCGI_Message const& msg) = delete;

  void finish() { m_finish(*this); }
  void finish(std::function<void(FCGI_Message const& msg)> const& fin) { m_finish = fin; }

 protected:
  virtual std::string response() const { return "FCGI-ed"; }
  bool done() {
    if (d) return true;
    d = !d;
    return false;
  }

 public:
  FCGI_Message() {}
};

class Server : public mkn::ram::tcp::SocketServer<uint8_t> {
 protected:
  int fdSize = _MKN_RAM_TCP_READ_BUFFER_;
  uint8_t m_acceptThreads, m_workerThreads;

  mkn::kul::Mutex m_actex, m_mutex, m_butex, m_mapex;
  mkn::kul::ChroncurrentThreadPool<> m_acceptPool;
  mkn::kul::ChroncurrentThreadPool<> m_workerPool;

  std::unordered_map<uint64_t, std::unique_ptr<FCGI_Message>> msgs;
  std::unordered_map<int, std::unique_ptr<uint8_t[]>> inBuffers;

 protected:
  uint8_t* getOrCreateBufferFor(int const& fd) {
    mkn::kul::ScopeLock lock(m_butex);
    if (!inBuffers.count(fd))
      inBuffers.insert(std::make_pair(fd, std::unique_ptr<uint8_t[]>(new uint8_t[fdSize])));
    return inBuffers[fd].get();
  }

  FCGI_Message& createFCGI_Message(int const& fd) {
    mkn::kul::ScopeLock lock(m_mapex);
    msgs[fd] = std::make_unique<FCGI_Message>();
    return *msgs[fd];
  }

  int accept(uint16_t const& fd) override {
    mkn::kul::ScopeLock lock(m_actex);
    return ::accept(lisock, (struct sockaddr*)&cli_addr[fd], &clilen);
  }

  void cycle(uint16_t const& size, std::map<int, uint8_t>* fds, int const& fd) {
    auto& msg(*msgs[fd]);
    work(msg);
    if (msg.done()) {
      uint8_t* out = getOrCreateBufferFor(fd);
      size_t size = FORM_RESPONSE(msg, out);
      write(*fds, fd, out, size);
    }
  }

  void write(std::map<int, uint8_t>& fds, int const& fd, uint8_t const* out, size_t const size);

  bool receive(std::map<int, uint8_t>& fds, int const& fd) override;

  void PARAMS(uint8_t* const in, int const& inLen, FCGI_Message& msg) {}

  void PARSE_FIRST(std::map<int, uint8_t>& fds, uint8_t* const in, int const& inLen, int const& fd)
      KTHROW(kul::fcgi::Exception);

  void PARSE(std::map<int, uint8_t>& fds, uint8_t* const in, int const& inLen, int const& fd,
             size_t pos) KTHROW(kul::fcgi::Exception);

  size_t FORM_RESPONSE(FCGI_Message const& msg, uint8_t* out);

  virtual void work(FCGI_Message& msg) {}

  void operateAccept(size_t const& threadID) {
    std::map<int, uint8_t> fds;
    fds.insert(std::make_pair(0, 0));
    for (size_t i = threadID; i < _MKN_RAM_TCP_MAX_CLIENT_; i += m_acceptThreads)
      fds.insert(std::make_pair(i, 0));
    while (s) try {
        mkn::kul::ScopeLock lock(m_mutex);
        loop(fds);
      } catch (mkn::ram::tcp::Exception const& e1) {
        KERR << e1.stack();
      } catch (std::exception const& e1) {
        KERR << e1.what();
      } catch (...) {
        KERR << "Loop Exception caught";
      }
  }
  void handleBuffer(std::map<int, uint8_t>& fds, int const& fd, uint8_t* in, int const& read,
                    int& e) {
    m_workerPool.async(std::bind(&Server::operateBuffer, std::ref(*this), &fds, fd, in, read, e),
                       std::bind(&Server::errorBuffer, std::ref(*this), std::placeholders::_1));
  }

  void operateBuffer(std::map<int, uint8_t>* fds, int const& fd, uint8_t* in, int const& read,
                     int& e) {
    PARSE_FIRST(*fds, in, read, fd);
    if (e < 0) {
      std::vector<int> del{fd};
      closeFDs(*fds, del);
    }
  }
  void errorBuffer(mkn::kul::Exception const& e) { KERR << e.stack(); };

 public:
  Server(uint16_t const& port, uint8_t const& acceptThreads = 1, uint8_t const& workerThreads = 1)
      : mkn::ram::tcp::SocketServer<uint8_t>(port),
        m_acceptThreads(acceptThreads),
        m_workerThreads(workerThreads),
        m_acceptPool(acceptThreads),
        m_workerPool(workerThreads) {
    if (acceptThreads < 1)
      KEXCEPTION("FCGI Server cannot have less than one threads for accepting");
    if (workerThreads < 1) KEXCEPTION("FCGI Server cannot have less than one threads for working");
  }

  virtual ~Server() {
    m_acceptPool.stop();
    m_workerPool.stop();
  }

  void start() KTHROW(mkn::ram::tcp::Exception) override;

  void join() {
    if (!started()) start();
    m_acceptPool.join();
    m_workerPool.join();
  }
  void stop() override {
    mkn::ram::tcp::SocketServer<uint8_t>::stop();
    m_acceptPool.stop();
    m_workerPool.stop();
  }
  void interrupt() {
    m_acceptPool.interrupt();
    m_workerPool.interrupt();
  }
};

}  // namespace fcgi
}  // namespace asio
}  // namespace ram
}  // namespace mkn

#endif /* _MKN_RAM_ASIO_FCGI_HPP_ */
