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
#ifndef _MKN_RAM_TCP_HPP_
#define _MKN_RAM_TCP_HPP_

#include "mkn/kul/tcp/def.hpp"

#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <map>
#include <unordered_set>

#include "mkn/kul/tcp.base.hpp"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

namespace mkn {
namespace ram {
namespace tcp {

template <class T = uint8_t>
class Socket : public ASocket<T> {
 protected:
  int iResult;

  WSADATA wsaData;
  SOCKET ConnectSocket = INVALID_SOCKET;
  struct addrinfo *result = NULL, *ptr = NULL, hints;

 public:
  virtual ~Socket() {
    if (this->open) close();
  }
  virtual bool connect(std::string const& host, int16_t const& port) override {
    KUL_DBG_FUNC_ENTER
    if (!CONNECT(*this, host, port)) return false;
    this->open = true;
    return true;
  }
  virtual bool close() override {
    KUL_DBG_FUNC_ENTER
    bool o1 = this->open;
    if (this->open) {
      this->open = 0;
      if (ConnectSocket == INVALID_SOCKET)
        KERR << "Socket shutdown: Socket is invalid ignoring";
      else {
        closesocket(ConnectSocket);
        WSACleanup();
      }
    }
    return o1;
  }
  virtual size_t read(T* data, size_t const& len) {
    bool more = false;
    return read(data, len, more);
  }
  virtual size_t read(T* data, size_t const& len, bool& more) {
    KUL_DBG_FUNC_ENTER

    int16_t d = recv(ConnectSocket, data, len, 0);

    return d;
  }
  virtual size_t write(T const* data, size_t const& len) override {
    iResult = send(ConnectSocket, data, len, 0);
    if (iResult == SOCKET_ERROR) KEXCEPTION("Socket send failed with error: ") << WSAGetLastError();
    return iResult;
  }

  SOCKET socket() { return ConnectSocket; }

 protected:
  static bool CONNECT(Socket& sck, std::string const& host, int16_t const& port) {
    KUL_DBG_FUNC_ENTER
    int16_t e = 0;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &sck.wsaData);
    if (iResult != 0) {
      KEXCEPTION("WSAStartup failed with error: ") << iResult;
      return 1;
    }

    ZeroMemory(&sck.hints, sizeof(sck.hints));
    sck.hints.ai_family = AF_UNSPEC;
    sck.hints.ai_socktype = SOCK_STREAM;
    sck.hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &sck.hints, &sck.result);
    if (iResult != 0) KEXCEPTION("getaddrinfo failed with error: ") << iResult;

    // Attempt to connect to an address until one succeeds
    for (sck.ptr = sck.result; sck.ptr != NULL; sck.ptr = sck.ptr->ai_next) {
      // Create a SOCKET for connecting to server
      sck.ConnectSocket = ::socket(sck.ptr->ai_family, sck.ptr->ai_socktype, sck.ptr->ai_protocol);
      if (sck.ConnectSocket == INVALID_SOCKET)
        KEXCEPTION("socket failed with error: ") << WSAGetLastError();

      // Connect to server.
      iResult = ::connect(sck.ConnectSocket, sck.ptr->ai_addr, (int)sck.ptr->ai_addrlen);
      if (iResult == SOCKET_ERROR) {
        closesocket(sck.ConnectSocket);
        sck.ConnectSocket = INVALID_SOCKET;
        continue;
      }
      break;
    }

    freeaddrinfo(sck.result);

    if (sck.ConnectSocket == INVALID_SOCKET) KEXCEPTION("Unable to connect to server!");

    return e >= 0;
  }
};

template <class T = uint8_t>
class SocketServer : public ASocketServer<T> {
 protected:
  bool s = 0;
  int iResult;
  int nfds = 1;
  int64_t _started;

  WSADATA wsaData;
  WSAPOLLFD m_fds[_MKN_RAM_TCP_MAX_CLIENT_];

  SOCKET lisock = INVALID_SOCKET;
  SOCKET ClientSocket = INVALID_SOCKET;

  struct addrinfo* result = NULL;
  struct addrinfo hints;

  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr[_MKN_RAM_TCP_MAX_CLIENT_];

  virtual bool handle(T* const in, size_t const& inLen, T* const out, size_t& outLen) {
    return true;
  }

  virtual int readFrom(int const& fd, T* in, int opts = 0) {
    return ::recv(m_fds[fd].fd, in, _MKN_RAM_TCP_READ_BUFFER_ - 1, opts);
  }
  virtual int writeTo(int const& fd, T const* const out, size_t size) {
    return ::send(m_fds[fd].fd, out, size, 0);
  }

  virtual bool receive(std::map<int, uint8_t>& fds, int const& fd) {
    KUL_DBG_FUNC_ENTER
    T in[_MKN_RAM_TCP_READ_BUFFER_];
    ZeroMemory(in, _MKN_RAM_TCP_READ_BUFFER_);

    bool cl;
    do {
      cl = 0;
      iResult = recv(ClientSocket, in, _MKN_RAM_TCP_READ_BUFFER_ - 1, 0);
      if (iResult > 0) {
        T out[_MKN_RAM_TCP_READ_BUFFER_];
        ZeroMemory(out, _MKN_RAM_TCP_READ_BUFFER_);
        size_t outLen;
        cl = handle(in, iResult, out, outLen);
        auto sent = writeTo(ClientSocket, out, strlen(out));
        if (sent == SOCKET_ERROR)
          KEXCEPTION("SocketServer send failed with error: ") << WSAGetLastError();
        if (cl) break;
      } else if (iResult == 0) {
        // printf("Connection closing...\n");
      } else
        KEXCEPTION("SocketServer recv failed with error: ") << WSAGetLastError();

    } while (iResult > 0);

    return true;
  }

  void closeFDsNoCompress(std::map<int, uint8_t>& fds, std::vector<int>& del) {
    KUL_DBG_FUNC_ENTER;
    for (auto const& fd : del) {
      ::closesocket(m_fds[fd].fd);
      m_fds[fd].fd = -1;
      fds[fd] = 0;
      nfds--;
    }
  }
  virtual void closeFDs(std::map<int, uint8_t>& fds, std::vector<int>& del) {
    closeFDsNoCompress(fds, del);
  }
  virtual void loop(std::map<int, uint8_t>& fds) KTHROW(kul::tcp::Exception) {
    auto ret = poll();
    if (!s) return;
    if (ret < 0)
      KEXCEPTION("Socket Server error on select: " + std::to_string(errno) + " - " +
                 std::string(strerror(errno)));
    // if(ret == 0) return;
    int newlisock = -1;
    ;
    for (auto const& pair : fds) {
      auto& i = pair.first;
      if (pair.second == 1) continue;
      if (m_fds[i].revents == 0) continue;
      if (m_fds[i].revents != POLLIN)
        KEXCEPTION("HTTP Server error on pollin " + std::to_string(m_fds[i].revents));

      if (m_fds[i].fd == lisock) {
        do {
          int newFD = nfds;
          while (1) {
            newFD++;
            if (fds.count(newFD) && !fds[newFD]) break;
          }
          newlisock = accept(newFD);
          if (newlisock < 0) {
            if (errno != EWOULDBLOCK) KEXCEPTION("SockerServer error on accept");
            break;
          }
          validAccept(fds, newlisock, newFD);
        } while (newlisock != -1);
      }
    }
    std::vector<int> del;
    for (auto const& pair : fds)
      if (pair.second == 1 && receive(fds, pair.first)) del.push_back(pair.first);
    if (del.size()) closeFDs(fds, del);
  }

  virtual SOCKET accept(int const& fd) {
    return WSAAccept(lisock, (struct sockaddr*)&cli_addr[fd], &clilen, NULL, NULL);
  }
  virtual void validAccept(std::map<int, uint8_t>& fds, int const& newlisock, int const& nfd) {
    KUL_DBG_FUNC_ENTER;
    KOUT(DBG) << "New connection , socket fd is " << newlisock
              << ", is : " << inet_ntoa(cli_addr[nfd].sin_addr)
              << ", port : " << ntohs(cli_addr[nfd].sin_port);
    this->onConnect(inet_ntoa(cli_addr[nfd].sin_addr), ntohs(cli_addr[nfd].sin_port));
    m_fds[nfd].fd = newlisock;
    m_fds[nfd].events = POLLIN;
    fds[nfd] = 1;
    nfds++;
  }

  virtual int poll(int timeout = 10) {
    auto p = WSAPoll(m_fds, nfds, timeout);

    return p;
  }

 public:
  SocketServer(uint16_t const& p, bool _bind = 1) : mkn::ram::tcp::ASocketServer<T>(p) {
    // if(_bind) bind(__MKN_RAM_TCP_BIND_SOCKTOPTS__);
  }
  void freeaddrinfo() {
    if (!result) return;
    ::freeaddrinfo(result);
    result = nullptr;
  }
  ~SocketServer() {
    freeaddrinfo();
    if (lisock) closesocket(lisock);
    if (ClientSocket) closesocket(ClientSocket);
    WSACleanup();
  }
  virtual void bind(int sockOpt = __MKN_RAM_TCP_BIND_SOCKTOPTS__) KTHROW(kul::Exception) {}
  virtual void start() KTHROW(kul::tcp::Exception) {
    KUL_DBG_FUNC_ENTER
    _started = mkn::kul::Now::MILLIS();

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) KEXCEPTION("WSAStartup failed with error: ") << iResult;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, std::to_string(port()).c_str(), &hints, &result);
    if (iResult != 0) {
      WSACleanup();
      KEXCEPTION("getaddrinfo failed with error: ") << iResult;
    }

    lisock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    int iso = 1;
    int rc = setsockopt(lisock, SOL_SOCKET, SO_REUSEADDR, (char*)&iso, sizeof(iso));
    if (lisock == INVALID_SOCKET) KEXCEPTION("socket failed with error: ") << WSAGetLastError();

    {
      unsigned long mode = 1;
      ioctlsocket(lisock, FIONBIO, &mode);
    }
    // Setup the TCP listening socket
    iResult = ::bind(lisock, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) KEXCEPTION("bind failed with error: ") << WSAGetLastError();

    freeaddrinfo();

    iResult = listen(lisock, SOMAXCONN);
    if (iResult == SOCKET_ERROR) KEXCEPTION("listen failed with error: ") << WSAGetLastError();

    s = true;
    std::map<int, uint8_t> fds;
    for (int i = 0; i < _MKN_RAM_TCP_MAX_CLIENT_; i++) fds.insert(std::make_pair(i, 0));
    try {
      while (s) loop(fds);
    } catch (mkn::ram::tcp::Exception const& e1) {
      KERR << e1.stack();
    } catch (std::exception const& e1) {
      KERR << e1.what();
    } catch (...) {
      KERR << "Loop Exception caught";
    }
  }
  virtual void stop() {
    KUL_DBG_FUNC_ENTER
    s = 0;
    ::closesocket(lisock);
    lisock = 0;
    ::closesocket(ClientSocket);
    ClientSocket = 0;
  }
};

}  // namespace tcp
}  // namespace ram
}  // namespace mkn

#endif  //_MKN_RAM_TCP_HPP_
