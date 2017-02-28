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
#ifdef  _KUL_HTTPS_
#include "kul/https.hpp"

void kul::https::Server::loop(std::unordered_set<int32_t>& fds) throw(kul::tcp::Exception){
    KUL_DBG_FUNC_ENTER
    fd_set l_fds = m_fds;

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;

    auto sel = select(_KUL_HTTPS_MAX_CLIENT_, &l_fds, &tv);
    if(sel < 0 && errno != EINTR) 
        KEXCEPTION("Socket Server error on select");
    if(sel == 0) return;
    if(FD_ISSET(sockfd, &l_fds)){
        int32_t newsockfd = accept();
        if(newsockfd < 0) KEXCEPTION("SockerServer error on accept");
        KOUT(DBG) << "New connection , socket fd is " << newsockfd << ", is : " << inet_ntoa(cli_addr.sin_addr) << ", port : "<< ntohs(cli_addr.sin_port);
        this->onConnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        fds.insert(newsockfd);
        FD_SET(newsockfd, &m_fds);
        ssl_clients[newsockfd] = SSL_new(ctx);
        if(!ssl_clients[newsockfd]) KEXCEPTION("HTTPS Server ssl failed to initialise");
        SSL_set_fd(ssl_clients[newsockfd], newsockfd); 
        int16_t ssl_err = SSL_accept(ssl_clients[newsockfd]);
        if(ssl_err <= 0){
            short se = 0;
            SSL_get_error(ssl_clients[newsockfd], se);
            SSL_shutdown(ssl_clients[newsockfd]);
            SSL_free(ssl_clients[newsockfd]);
            close(newsockfd);
            KERR << "HTTPS Server SSL ERROR on SSL_ACCEPT error: " << se;
            KEXCEPTION("HTTPS Server SSL ERROR on SSL_ACCEPT error");
        }
        X509* cc = SSL_get_peer_certificate (ssl_clients[newsockfd]);
        if(cc != NULL) {
            KLOG(DBG) << "Client certificate:";
            KLOG(DBG) << "\t subject: " << X509_NAME_oneline (X509_get_subject_name (cc), 0, 0);
            KLOG(DBG) << "\t issuer: %s\n" << X509_NAME_oneline (X509_get_issuer_name  (cc), 0, 0);
            X509_free(cc);
        }else KLOG(ERR) << "Client does not have certificate.";
    }
    for(uint16_t i = 0; i < _KUL_HTTPS_MAX_CLIENT_; i++)
        if(fds.count(i) && FD_ISSET(i, &l_fds))
            receive(i);
}

void kul::https::Server::setChain(const kul::File& f){
    if(!f) KEXCEPTION("HTTPS Server chain file does not exist: " + f.full());
    if(SSL_CTX_use_certificate_chain_file(ctx, f.mini().c_str()) <= 0)
        KEXCEPTION("HTTPS Server SSL_CTX_use_PrivateKey_file failed");
}

kul::https::Server& kul::https::Server::init(){
    KUL_DBG_FUNC_ENTER
    if(!crt) KEXCEPTION("HTTPS Server crt file does not exist: " + crt.full());
    if(!key) KEXCEPTION("HTTPS Server key file does not exist: " + key.full());
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ctx = SSL_CTX_new(TLSv1_2_server_method());
    if (!ctx) KEXCEPTION("HTTPS Server SSL_CTX failed SSL_CTX_new");
    if(SSL_CTX_use_certificate_file(ctx, crt.mini().c_str(), SSL_FILETYPE_PEM) <= 0)
        KEXCEPTION("HTTPS Server SSL_CTX_use_certificate_file failed");
    if(SSL_CTX_use_PrivateKey_file(ctx, key.mini().c_str(), SSL_FILETYPE_PEM) <= 0)
        KEXCEPTION("HTTPS Server SSL_CTX_use_PrivateKey_file failed");
    if (!SSL_CTX_check_private_key(ctx))
        KEXCEPTION("HTTPS Server SSL_CTX_check_private_key failed");
    if(!cs.empty() && !SSL_CTX_set_cipher_list(ctx, cs.c_str()))
        KEXCEPTION("HTTPS Server SSL_CTX_set_cipher_listctx failed");
    return *this;
}

void kul::https::Server::stop(){
    KUL_DBG_FUNC_ENTER
    s = 0;
    ERR_free_strings();
    EVP_cleanup();
    for(size_t i = 0; i < _KUL_HTTPS_MAX_CLIENT_; i++){
        auto ssl = ssl_clients[i];
        if(ssl){
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
    }
    if(ctx) SSL_CTX_free(ctx);
    kul::http::Server::stop();
}


void kul::https::Server::receive(const uint16_t& fd){
    KUL_DBG_FUNC_ENTER
    char buffer[_KUL_HTTPS_READ_BUFFER_];
    bzero(buffer, _KUL_HTTPS_READ_BUFFER_);
    int16_t e = 0, read = ::SSL_read(ssl_clients[fd], buffer, _KUL_HTTPS_READ_BUFFER_ - 1);
    if(read == 0){
        getpeername(fd , (struct sockaddr*) &cli_addr , (socklen_t*)&clilen);
        KOUT(DBG) << "Host disconnected , ip: " << inet_ntoa(serv_addr.sin_addr) << ", port " << ntohs(serv_addr.sin_port);
        onDisconnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        SSL_shutdown(ssl_clients[fd]);
        SSL_free(ssl_clients[fd]);
        ssl_clients[fd] = 0;
        close(fd);
        FD_CLR(fd, &m_fds);
    }else{
        buffer[read] = '\0';
        std::string res;
        try{
            std::string s(buffer);
            std::string c(s.substr(0, (s.size() > 9) ? 10 : s.size()));
            std::vector<char> allowed = {'D', 'G', 'P', '/', 'H'};
            bool f = 0;
            for(const auto& ch : allowed){
                f = c.find(ch) != std::string::npos;
                if(f) break;
            }
            if(!f) KEXCEPTION("Logic error encountered, probably https attempt on http port");
            
            std::shared_ptr<kul::http::ARequest> req = handleRequest(s, res);
            const kul::http::AResponse& rs(respond(*req.get()));
            std::string ret(rs.toString());
            e = ::SSL_write(ssl_clients[fd], ret.c_str(), ret.length());
        }catch(const kul::http::Exception& e1){
            KERR << e1.stack(); 
            e = -1;
        }
        if(e < 0){
            KLOG(ERR) << "Error replying to host errno: " << strerror(errno);
            SSL_shutdown(ssl_clients[fd]);
            SSL_free(ssl_clients[fd]);
            ssl_clients[fd] = 0;
            close(fd);
            FD_CLR(fd, &m_fds);
        }
    }
}

#endif//_KUL_HTTPS_