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
#ifdef _KUL_INCLUDE_FCGI_

#include "kul/asio/fcgi.hpp"

#include "kul/http.hpp"

void kul::asio::fcgi::Server::start() KTHROW(Exception) {
  KUL_DBG_FUNC_ENTER
  nfds = lisock + 1;

  _started = kul::Now::MILLIS();
  listen(lisock, 256);
  clilen = sizeof(cli_addr);
  s = true;
  m_fds[0].fd = lisock;
  m_fds[0].events = POLLIN;  //|POLLPRI;
  nfds = lisock + 1;

  for (size_t i = 0; i < m_acceptThreads; i++)
    m_acceptPool.async(std::bind(&Server::operateAccept, std::ref(*this), i));

  m_acceptPool.start();
  m_workerPool.start();
}

bool kul::asio::fcgi::Server::receive(std::map<int, uint8_t>& fds,
                                      const int& fd) {
  KUL_DBG_FUNC_ENTER
  KLOG(INF);
  uint8_t* in = getOrCreateBufferFor(fd);
  bzero(in, _KUL_TCP_READ_BUFFER_);
  KLOG(INF);
  int e = 0, read = readFrom(fd, in, MSG_DONTWAIT);
  KLOG(INF) << read;
  if (read < 0)
    e = -1;
  else if (read > 0) {
    fds[fd] = 2;
    handleBuffer(fds, fd, in, read, e);
    if (e) return false;
  } else {
    getpeername(m_fds[fd].fd, (struct sockaddr*)&cli_addr, (socklen_t*)&clilen);
    onDisconnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
  }
  if (e < 0) KLOG(ERR) << "Error on receive: " << strerror(errno);
  return false;
}

void kul::asio::fcgi::Server::write(std::map<int, uint8_t>& fds, const int& fd,
                                    const uint8_t* out, const size_t size) {
  KUL_DBG_FUNC_ENTER

  KLOG(INF) << size;
  writeTo(fd, out, size);
  receive(fds, fd);
  std::vector<int> del{fd};
  closeFDs(fds, del);
}

void kul::asio::fcgi::Server::PARSE_FIRST(std::map<int, uint8_t>& fds,
                                          uint8_t* const in, const int& inLen,
                                          const int& fd)
    KTHROW(kul::fcgi::Exception) {
  KUL_DBG_FUNC_ENTER

  if (inLen < 4) KEXCEPTION("FCGI cannot parse input too short");

  const uint8_t& type = in[1];
  if (type == FCGI_BEGIN_REQUEST) {
    uint16_t size = (in[5] | in[4] << 8);
    auto& msg = createFCGI_Message(fd);
    msg.rid = (in[3] | in[2] << 8);
    msg.finish([&](const FCGI_Message& msg) {
      uint8_t* out = getOrCreateBufferFor(fd);
      size_t size = FORM_RESPONSE(msg, out);
      write(fds, fd, out, size);
    });
    return PARSE(fds, in, inLen, fd, 8 + size);
  } else {
    uint8_t f[] = {'F', 'A', 'I', 'L'};
    size_t s = 4;
    write(fds, fd, f, s);
  }
}

void kul::asio::fcgi::Server::PARSE(std::map<int, uint8_t>& fds,
                                    uint8_t* const in, const int& inLen,
                                    const int& fd, size_t pos)
    KTHROW(kul::fcgi::Exception) {
  KUL_DBG_FUNC_ENTER

  if ((inLen - pos) < 4) KEXCEPTION("FCGI cannot parse input too short");

  const uint8_t& type = in[pos + 1];
  if (type == FCGI_PARAMS) {
    uint16_t size = (in[pos + 5] | in[pos + 4] << 8);
    uint8_t pad = in[pos + 6];
    if (size == 0) return PARSE(fds, in, inLen, fd, pos + 8);
    pos += 8;
    auto& msg(*msgs[fd]);
    PARAMS(in + pos, size, msg);
    return PARSE(fds, in, inLen, fd, pos + pad + size);
  } else if (type == FCGI_STDIN) {
    uint16_t size = (in[pos + 5] | in[pos + 4] << 8);
    if (size == 0) {
      m_workerPool.async(
          std::bind(&Server::cycle, std::ref(*this), size, &fds, fd));
    } else {
      // auto& msg(msgs[rid]);
      // msg.body(FORM_REQUEST(in, inLen, pos));
    }
  } else {
    uint8_t f[] = {'F', 'A', 'I', 'L'};
    size_t s = 4;
    write(fds, fd, f, s);
  }
}

size_t kul::asio::fcgi::Server::FORM_RESPONSE(const FCGI_Message& msg,
                                              uint8_t* out) {
  KUL_DBG_FUNC_ENTER
  bzero(out, _KUL_TCP_READ_BUFFER_);

  size_t pos = 0;
  out[pos++] = 1;
  out[pos++] = 6;
  uint8_t rid[2];
  {
    const void* vp = &msg.rid;
    rid[0] = *static_cast<const uint8_t*>(vp);
    rid[1] = *static_cast<const uint8_t*>(vp + sizeof(uint8_t));

    out[pos++] = rid[1];
    out[pos++] = rid[0];
  }

  kul::http::_1_1Response resp(msg.response());
  resp.header("Content-Type", "text/html");
  std::string response(resp.toString());
  auto ending(response.find("\r\n"));
  if (ending != std::string::npos) response.erase(ending, ending + 2);
  response.pop_back();
  response.pop_back();
  response.pop_back();

  auto size = response.size();
  {
    void* vp = &size;

    uint8_t sp1 = *static_cast<uint8_t*>(vp);
    uint8_t sp2 = *static_cast<uint8_t*>(vp + sizeof(uint8_t));

    out[pos++] = sp2;
    out[pos++] = sp1;
  }

  uint8_t pad = 8 - (size % 8);
  if (pad == 8) pad = 0;
  out[pos++] = pad;
  out[pos++] = 0;

  std::memcpy(out + pos, response.c_str(), response.size());
  pos += response.size();

  for (uint8_t i = 0; i < pad; i++) out[pos++] = 0;

  // blank
  {
    out[pos++] = 1;
    out[pos++] = 6;
    out[pos++] = rid[1];
    out[pos++] = rid[0];
    for (uint8_t i = 0; i < 4; i++) out[pos++] = 0;
  }

  // end
  {
    out[pos++] = 1;
    out[pos++] = 3;
    out[pos++] = rid[1];
    out[pos++] = rid[0];
    out[pos++] = 0;
    out[pos++] = 8;
    for (uint8_t i = 0; i < 10; i++) out[pos++] = 0;
  }

  return size + pad + 40;
}

#endif  //_KUL_INCLUDE_FCGI_
