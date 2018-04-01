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
#include "kul/http.hpp"

bool kul::http::Server::receive(std::map<int, uint8_t>& fds, const int& fd) {
  KUL_DBG_FUNC_ENTER;
  char* in = getOrCreateBufferFor(fd);
  bzero(in, _KUL_TCP_READ_BUFFER_);
  int e = 0, read = readFrom(fd, in);
  if (read < 0)
    e = -1;
  else if (read > 0) {
    fds[fd] = 2;
    handleBuffer(fds, fd, in, read, e);
    if (e) return false;
  } else {
    getpeername(m_fds[fd].fd, (struct sockaddr*)&cli_addr, (socklen_t*)&clilen);
    onDisconnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    KOUT(DBG) << "DISCO,  " << inet_ntoa(cli_addr.sin_addr)
              << ", port : " << ntohs(cli_addr.sin_port);
  }
  if (e < 0) KLOG(ERR) << "Error on receive: " << strerror(errno);
  return true;
}
