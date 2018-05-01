/**
Copyright (c) 2013, Philip Deegan.
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
#ifndef _KUL_TCP_HPP_
#define _KUL_TCP_HPP_

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>
#include <unordered_set>

#include "kul/byte.hpp"
#include "kul/log.hpp"
#include "kul/tcp.base.hpp"
#include "kul/tcp/def.hpp"
#include "kul/time.hpp"

#ifndef __KUL_TCP_BIND_SOCKTOPTS__
#define __KUL_TCP_BIND_SOCKTOPTS__ SO_REUSEADDR
#endif  //__KUL_TCP_BIND_SOCKTOPTS__

namespace kul {
namespace tcp {

template <class T = uint8_t>
class Socket : public ASocket<T> {
 protected:
  int sck = 0;

 public:
  virtual ~Socket() {
    if (this->open) close();
  }
  virtual bool connect(const std::string& host, const int16_t& port) override {
    KUL_DBG_FUNC_ENTER
    if (!SOCKET(sck) || !CONNECT(sck, host, port)) return false;
    this->open = true;
    return true;
  }
  virtual bool close() override {
    KUL_DBG_FUNC_ENTER
    bool o1 = this->open;
    if (this->open) {
      ::close(sck);
      this->open = 0;
    }
    return o1;
  }
  virtual size_t read(T* data, const size_t& len) {
    bool more = false;
    return read(data, len, more);
  }
  virtual size_t read(T* data, const size_t& len, bool& more)
      KTHROW(kul::tcp::Exception) override {
    KUL_DBG_FUNC_ENTER
    struct timeval tv;
    fd_set fds;
    int64_t ret = 0, iof = -1;
    bool r = 0;
    FD_ZERO(&fds);
    FD_SET(sck, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 500;
    ret = select(sck + 1, &fds, NULL, NULL, &tv);
    std::function<bool(int64_t&, bool)> check_error;
    check_error = [&](int64_t& ret, bool peek = false) -> bool {
      ret = 0;
      int error = 0;  // or WSAGetLastError()
      if (peek) {
        ret = recv(sck, data, len, MSG_PEEK);
        error = errno;  // or WSAGetLastError()
      } else {
        ret = recv(sck, data, len, 0);
        error = errno;  // or WSAGetLastError()
      }
      if (ret > 0) {
        return false;
      } else if (ret == 0) {

      } else {
        // there is an error, let's see what it is
        // error = errno; // or WSAGetLastError()
        switch (error) {
          // case EAGAIN:
          case EWOULDBLOCK:
            // Socket is O_NONBLOCK and there is no data available
          case EINTR:
            // an interrupt (signal) has been catched
            // should be ingore in most cases
            break;
          default:
            // socket has an error, no valid anymore
            break;
        }
      }
      return false;
    };

    if (ret < 0)
      KEXCEPTION("Failed to read from Server socket " +
                 std::string(strerror(errno)));
    else if (ret > 0 && FD_ISSET(sck, &fds)) {
      if ((iof = fcntl(sck, F_GETFL, 0)) != -1)
        fcntl(sck, F_SETFL, iof | O_NONBLOCK);
      int64_t size = 0;
      while (check_error(ret, false)) {
      }
      if (ret > 0) ret += read(data + ret, len - ret, more);
      ioctl(sck, FIONREAD, &size);
      more = size > 0;
      if (iof != -1) fcntl(sck, F_SETFL, iof);
    } else if (ret == 0 && !FD_ISSET(sck, &fds)) {
      while (check_error(ret, false)) {
      }
      if (ret > 0) ret += read(data + ret, len - ret, more);
      if (!r && !ret) KEXCEPTION("Failed to read from Server socket");
    }
    return ret;
  }
  virtual size_t write(const T* data, const size_t& len) override {
    return ::send(sck, data, len, 0);
  }

  static bool SOCKET(int& sck, const int16_t& domain = AF_INET,
                     const int16_t& type = SOCK_STREAM,
                     const int16_t& protocol = IPPROTO_TCP) {
    KUL_DBG_FUNC_ENTER
    sck = socket(domain, type, protocol);
    if (sck < 0) KLOG(ERR) << "SOCKET ERROR CODE: " << sck;
    return sck >= 0;
  }
  static bool CONNECT(const int& sck, const std::string& host,
                      const int16_t& port) {
    KUL_DBG_FUNC_ENTER
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    int16_t e = 0;
    servAddr.sin_port = !kul::byte::isBigEndian()
                            ? htons(port)
                            : kul::byte::LittleEndian::UINT32(port);
    if (host == "localhost" || host == "127.0.0.1") {
      servAddr.sin_addr.s_addr = INADDR_ANY;
      e = ::connect(sck, (struct sockaddr*)&servAddr, sizeof(servAddr));
      if (e < 0)
        KLOG(ERR) << "SOCKET CONNECT ERROR CODE: " << e << " errno: " << errno;
    } else if (inet_pton(AF_INET, &host[0], &(servAddr.sin_addr))) {
      e = ::connect(sck, (struct sockaddr*)&servAddr, sizeof(servAddr));
      if (e < 0)
        KLOG(ERR) << "SOCKET CONNECT ERROR CODE: " << e << " errno: " << errno;
    } else {
      std::string ip;
      struct addrinfo hints, *servinfo, *next;
      struct sockaddr_in* in;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;  // use AF_INET6 to force IPv6
      hints.ai_socktype = SOCK_STREAM;
      if ((e = getaddrinfo(host.c_str(), 0, &hints, &servinfo)) != 0)
        KEXCEPTION("getaddrinfo failed for host: " + host);
      for (next = servinfo; next != NULL; next = next->ai_next) {
        in = (struct sockaddr_in*)next->ai_addr;
        ip = inet_ntoa(in->sin_addr);
        servAddr.sin_addr.s_addr = inet_addr(&ip[0]);
        if (ip == "0.0.0.0") continue;
        if ((e = ::connect(sck, (struct sockaddr*)&servAddr,
                           sizeof(servAddr))) == 0)
          break;
      }
      freeaddrinfo(servinfo);
      if (e < 0)
        KLOG(ERR) << "SOCKET CONNECT ERROR CODE: " << e << " errno: " << errno;
    }
    return e >= 0;
  }
};

template <class T = uint8_t>
class SocketServer : public ASocketServer<T> {
 protected:
  bool s = 0;
  int lisock = 0, nfds = 12;
  int64_t _started;
  struct pollfd m_fds[_KUL_TCP_MAX_CLIENT_];
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr[_KUL_TCP_MAX_CLIENT_];

  virtual bool handle(T* const in, const size_t& inLen, T* const out,
                      size_t& outLen) {
    return true;
  }

  virtual int readFrom(const int& fd, T* in, int opts = 0) {
    return ::recv(m_fds[fd].fd, in, _KUL_TCP_READ_BUFFER_ - 1, opts);
  }
  virtual int writeTo(const int& fd, const T* const out, size_t size) {
    return ::send(m_fds[fd].fd, out, size, 0);
  }
  virtual bool receive(std::map<int, uint8_t>& fds, const int& fd) {
    KUL_DBG_FUNC_ENTER
    T in[_KUL_TCP_READ_BUFFER_];
    bzero(in, _KUL_TCP_READ_BUFFER_);
    int16_t e = 0, read = readFrom(fd, in);
    if (read < 0 && errno != EWOULDBLOCK)
      KEXCEPTION("Socket Server error on recv - fd(" + std::to_string(fd) +
                 ") : " + std::to_string(errno) + " - " +
                 std::string(strerror(errno)));
    if (read == 0) {
      getpeername(m_fds[fd].fd, (struct sockaddr*)&cli_addr[fd],
                  (socklen_t*)&clilen);
      KOUT(DBG) << "Host disconnected , ip: " << inet_ntoa(serv_addr.sin_addr)
                << ", port " << ntohs(serv_addr.sin_port);
      this->onDisconnect(inet_ntoa(cli_addr[fd].sin_addr),
                         ntohs(cli_addr[fd].sin_port));
      return true;
    } else {
      bool cl = 1;
      in[read] = '\0';
      try {
        T out[_KUL_TCP_READ_BUFFER_];
        bzero(out, _KUL_TCP_READ_BUFFER_);
        size_t outLen;
        cl = handle(in, read, out, outLen);
        e = writeTo(fd, out, outLen);
      } catch (const kul::tcp::Exception& e1) {
        KERR << e1.stack();
        e = -1;
      }
      if (e < 0)
        KLOG(ERR) << "Error replying to host errno: " << strerror(errno);
      if (e < 0 || cl) return true;
    }
    return false;
  }
  void closeFDsNoCompress(std::map<int, uint8_t>& fds, std::vector<int>& del) {
    KUL_DBG_FUNC_ENTER;
    for (const auto& fd : del) {
      ::close(m_fds[fd].fd);
      m_fds[fd].fd = -1;
      fds[fd] = 0;
      nfds--;
    }
  }
  virtual void closeFDs(std::map<int, uint8_t>& fds, std::vector<int>& del) {
    closeFDsNoCompress(fds, del);
  }
  virtual void loop(std::map<int, uint8_t>& fds) KTHROW(kul::tcp::Exception) {
    KUL_DBG_FUNC_ENTER
    auto ret = poll();
    if (!s) return;
    if (ret < 0)
      KEXCEPTION("Socket Server error on select: " + std::to_string(errno) +
                 " - " + std::string(strerror(errno)));
    // if(ret == 0) return;
    int newlisock = -1;
    ;
    for (const auto& pair : fds) {
      auto& i = pair.first;
      if (pair.second == 1) continue;
      if (m_fds[i].revents == 0) continue;
      if (m_fds[i].revents != POLLIN)
        KEXCEPTION("HTTP Server error on pollin " +
                   std::to_string(m_fds[i].revents));

      if (m_fds[i].fd == lisock) {
        do {
          int newFD = nfds;
          while (1) {
            newFD++;
            if (fds.count(newFD) && !fds[newFD]) break;
          }
          newlisock = accept(newFD);
          if (newlisock < 0) {
            if (errno != EWOULDBLOCK)
              KEXCEPTION("SockerServer error on accept");
            break;
          }
          validAccept(fds, newlisock, newFD);
        } while (newlisock != -1);
      }
    }
    std::vector<int> del;
    for (const auto& pair : fds)
      if (pair.second == 1 && receive(fds, pair.first))
        del.push_back(pair.first);
    if (del.size()) closeFDs(fds, del);
  }
  virtual int poll(int timeout = 10) {
    auto p = ::poll(m_fds, nfds, timeout);
    if (errno == 11) {
      kul::this_thread::sleep(timeout);
      return 0;
    } else if (errno == 2 || errno == 17 || errno == 32) {
    } else if (errno) {
      KLOG(ERR) << std::to_string(errno) << " - "
                << std::string(strerror(errno));
      return -1;
    }
    return p;
  }
  virtual int accept(const int& fd) {
    return ::accept(lisock, (struct sockaddr*)&cli_addr[fd], &clilen);
  }
  virtual void validAccept(std::map<int, uint8_t>& fds, const int& newlisock,
                           const int& nfd) {
    KUL_DBG_FUNC_ENTER;
    KOUT(DBG) << "New connection , socket fd is " << newlisock
              << ", is : " << inet_ntoa(cli_addr[nfd].sin_addr)
              << ", port : " << ntohs(cli_addr[nfd].sin_port);
    this->onConnect(inet_ntoa(cli_addr[nfd].sin_addr),
                    ntohs(cli_addr[nfd].sin_port));
    m_fds[nfd].fd = newlisock;
    m_fds[nfd].events = POLLIN;
    fds[nfd] = 1;
    nfds++;
  }

 public:
  SocketServer(const uint16_t& p, bool _bind = 1)
      : kul::tcp::ASocketServer<T>(p) {
    if (_bind) bind(__KUL_TCP_BIND_SOCKTOPTS__);
    memset(m_fds, 0, sizeof(m_fds));
  }
  ~SocketServer() {
    for (int i = 0; i < _KUL_TCP_MAX_CLIENT_; i++) ::close(m_fds[i].fd);
  }
  virtual void bind(int sockOpt = __KUL_TCP_BIND_SOCKTOPTS__)
      KTHROW(kul::Exception) {
    lisock = socket(AF_INET, SOCK_STREAM, 0);
    int iso = 1;
    int rc = setsockopt(lisock, SOL_SOCKET, sockOpt, (char*)&iso, sizeof(iso));
    if (rc < 0) {
      KERR << std::to_string(errno) << " - " << std::string(strerror(errno));
      KEXCEPTION("Socket Server error on setsockopt");
    }

#if defined(O_NONBLOCK)
    if (-1 == (iso = fcntl(lisock, F_GETFL, 0))) iso = 0;
    rc = fcntl(lisock, F_SETFL, iso | O_NONBLOCK);
#else
    rc = ioctl(lisock, FIONBIO, (char*)&iso);
#endif
    if (rc < 0) KEXCEPTION("Socket Server error on ioctl");
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = !kul::byte::isBigEndian()
                             ? htons(this->port())
                             : kul::byte::LittleEndian::UINT32(this->port());
    int16_t e = 0;
    if ((e = ::bind(lisock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) <
        0) {
      KERR << std::to_string(errno) << " - " << std::string(strerror(errno));
      KEXCEPTION("Socket Server error on binding, errno: " +
                 std::to_string(errno));
    }
  }
  virtual void start() KTHROW(kul::tcp::Exception) {
    KUL_DBG_FUNC_ENTER
    _started = kul::Now::MILLIS();
    auto ret = listen(lisock, 256);
    if (ret < 0) KEXCEPTION("Socket Server error on listen");
    clilen = sizeof(cli_addr[0]);
    s = true;
    m_fds[0].fd = lisock;
    m_fds[0].events = POLLIN;  //|POLLPRI;
    nfds = lisock + 1;
    std::map<int, uint8_t> fds;
    for (size_t i = 0; i < _KUL_TCP_MAX_CLIENT_; i++)
      fds.insert(std::make_pair(i, 0));
    try {
      while (s) loop(fds);
    } catch (const kul::tcp::Exception& e1) {
      KERR << e1.stack();
    } catch (const std::exception& e1) {
      KERR << e1.what();
    } catch (...) {
      KERR << "Loop Exception caught";
    }
  }
  virtual void stop() {
    KUL_DBG_FUNC_ENTER
    s = 0;
    ::close(lisock);
    for (int i = 0; i < _KUL_TCP_MAX_CLIENT_; i++)
      if (i != lisock) shutdown(i, SHUT_RDWR);
  }
};

}  // END NAMESPACE tcp
}  // END NAMESPACE kul

#endif  //_KUL_TCP_HPP_
