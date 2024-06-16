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
#ifdef _MKN_RAM_INCLUDE_HTTPS_
#include "mkn/ram/https.hpp"

void mkn::ram::https::Server::loop(std::map<int, uint8_t>& fds) KTHROW(kul::tcp::Exception) {
  KUL_DBG_FUNC_ENTER

  auto ret = poll(1000);

  if (!s) return;
  if (ret < 0)
    KEXCEPTION("HTTPS Server error on poll: " + std::to_string(errno) + " - " +
               std::string(strerror(errno)));
  if (ret == 0) return;
  int newlisock = -1;
  ;
  for (const auto& pair : fds) {
    auto& i = pair.first;
    if (pair.second == 1) continue;
    if (m_fds[i].revents == 0) continue;
    if (m_fds[i].revents != POLLIN)
      KEXCEPTION("HTTPS Server error on pollin " + std::to_string(m_fds[i].revents));

    if (m_fds[i].fd == lisock) {
      do {
        int newFD = nfds;
        while (1) {
          newFD++;
          if (fds.count(newFD) && !fds[newFD]) break;
        }
        newlisock = accept(newFD);
        KLOG(DBG) << "lisock: " << lisock << ", newlisock: " << newlisock;
        if (newlisock < 0) {
          if (errno != EWOULDBLOCK) KEXCEPTION("HTTPS Server error on accept");
          break;
        }
        ssl_clients[newlisock] = SSL_new(ctx);
        if (!ssl_clients[newlisock]) KEXCEPTION("HTTPS Server ssl failed to initialise");
        SSL_set_fd(ssl_clients[newlisock], newlisock);
        int16_t ssl_err = SSL_accept(ssl_clients[newlisock]);
        if (ssl_err <= 0) {
          short se = 0;
          SSL_get_error(ssl_clients[newlisock], se);
          SSL_shutdown(ssl_clients[newlisock]);
          SSL_free(ssl_clients[newlisock]);
          ::closesocket(newlisock);
          KERR << "HTTPS Server SSL ERROR on SSL_ACCEPT error: " << ssl_err << " :" << se;
          KEXCEPTION("HTTPS Server SSL ERROR on SSL_ACCEPT error");
        }
        X509* cc = SSL_get_peer_certificate(ssl_clients[newlisock]);
        if (cc != NULL) {
          KLOG(DBG) << "Client certificate:";
          KLOG(DBG) << "\t subject: " << X509_NAME_oneline(X509_get_subject_name(cc), 0, 0);
          KLOG(DBG) << "\t issuer: %s\n" << X509_NAME_oneline(X509_get_issuer_name(cc), 0, 0);
          X509_free(cc);
        }  // else KLOG(ERR) << "Client does not have certificate.";

        validAccept(fds, newlisock, newFD);
      } while (newlisock != -1);
    }
  }
  std::vector<int> del;
  for (const auto& pair : fds) {
    if (pair.second == 1 && receive(fds, pair.first)) del.push_back(pair.first);
  }
  if (del.size()) closeFDs(fds, del);
}

void mkn::ram::https::Server::setChain(const mkn::kul::File& f) {
  if (!f) KEXCEPTION("HTTPS Server chain file does not exist: " + f.full());
  if (SSL_CTX_use_certificate_chain_file(ctx, f.mini().c_str()) <= 0)
    KEXCEPTION("HTTPS Server SSL_CTX_use_PrivateKey_file failed");
}

mkn::ram::https::Server& mkn::ram::https::Server::init() {
  KUL_DBG_FUNC_ENTER
  if (!crt) KEXCEPTION("HTTPS Server crt file does not exist: " + crt.full());
  if (!key) KEXCEPTION("HTTPS Server key file does not exist: " + key.full());
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  ctx = SSL_CTX_new(_MKN_RAM_HTTPS_SERVER_METHOD_());
  if (!ctx) KEXCEPTION("HTTPS Server SSL_CTX failed SSL_CTX_new");
  if (SSL_CTX_use_certificate_file(ctx, crt.mini().c_str(), SSL_FILETYPE_PEM) <= 0)
    KEXCEPTION("HTTPS Server SSL_CTX_use_certificate_file failed");
  if (SSL_CTX_use_PrivateKey_file(ctx, key.mini().c_str(), SSL_FILETYPE_PEM) <= 0)
    KEXCEPTION("HTTPS Server SSL_CTX_use_PrivateKey_file failed");
  if (!SSL_CTX_check_private_key(ctx)) KEXCEPTION("HTTPS Server SSL_CTX_check_private_key failed");
  if (!cs.empty() && !SSL_CTX_set_cipher_list(ctx, cs.c_str()))
    KEXCEPTION("HTTPS Server SSL_CTX_set_cipher_listctx failed");
  return *this;
}

void mkn::ram::https::Server::stop() {
  KUL_DBG_FUNC_ENTER
  s = 0;
  ERR_free_strings();
  EVP_cleanup();
  for (size_t i = 0; i < _MKN_RAM_TCP_MAX_CLIENT_; i++) {
    auto ssl = ssl_clients[i];
    if (ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
    }
  }
  if (ctx) SSL_CTX_free(ctx);
  mkn::ram::http::Server::stop();
}

void mkn::ram::https::Server::handleBuffer(std::map<int, uint8_t>& fds, const int& fd, char* in,
                                           const int& read, int& e) {
  in[read] = '\0';
  std::string res;
  try {
    std::string s(in);
    std::string c(s.substr(0, (s.size() > 9) ? 10 : s.size()));
    std::vector<char> allowed = {'D', 'G', 'P', '/', 'H'};
    bool f = 0;
    for (const auto& ch : allowed) {
      f = c.find(ch) != std::string::npos;
      if (f) break;
    }
    if (!f) KEXCEPTION("Logic error encountered, probably https attempt on http port");
    std::shared_ptr<mkn::ram::http::A1_1Request> req = handleRequest(fd, s, res);
    const mkn::ram::http::_1_1Response& rs(respond(*req.get()));
    std::string ret(rs.toString());
    e = ::SSL_write(ssl_clients[m_fds[fd].fd], ret.c_str(), ret.length());
  } catch (const mkn::ram::http::Exception& e1) {
    KERR << e1.stack();
    e = -1;
  }
  fds[fd] = 1;
}

bool mkn::ram::https::Server::receive(std::map<int, uint8_t>& fds, const int& fd) {
  KUL_DBG_FUNC_ENTER
  char* in = getOrCreateBufferFor(fd);
  bzero(in, _MKN_RAM_TCP_READ_BUFFER_);
  int e = 0, read = ::SSL_read(ssl_clients[m_fds[fd].fd], in, _MKN_RAM_TCP_READ_BUFFER_ - 1);
  if (read < 0)
    e = -1;
  else if (read > 0) {
    fds[fd] = 2;
    handleBuffer(fds, fd, in, read, e);
    if (e) return false;
  } else {
    getpeername(m_fds[fd].fd, (struct sockaddr*)&cli_addr, (socklen_t*)&clilen);
    onDisconnect(inet_ntoa(cli_addr[fd].sin_addr), ntohs(cli_addr[fd].sin_port));
  }
  if (e < 0) KLOG(ERR) << "Error on receive: " << strerror(errno);
  SSL_shutdown(ssl_clients[m_fds[fd].fd]);
  SSL_free(ssl_clients[m_fds[fd].fd]);
  ssl_clients[m_fds[fd].fd] = 0;
  return true;
}

#endif  //_MKN_RAM_INCLUDE_HTTPS_