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


void kul::https::Server::loop() throw(kul::tcp::Exception){
    int32_t newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if(newsockfd < 0) KEXCEPTION("HTTPS Server error on accept");

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, newsockfd);
    //Here is the SSL Accept portion.  Now all reads and writes must use SSL
    int16_t ssl_err = SSL_accept(ssl);
    if(ssl_err <= 0){
        short se = 0;
        SSL_get_error(ssl, se);
        KERR << "HTTPS Server SSL ERROR on SSL_ACCEPT error: " << se;
        close(newsockfd);
        return;
    }

    KLOG(DBG) << "SSL_get_cipher: " << SSL_get_cipher(ssl);
    cc = SSL_get_peer_certificate (ssl);

    if(cc != NULL) {
        KLOG(DBG) << "Client certificate:";
        KLOG(DBG) << "\t subject: " << X509_NAME_oneline (X509_get_subject_name (cc), 0, 0);
        KLOG(DBG) << "\t issuer: %s\n" << X509_NAME_oneline (X509_get_issuer_name  (cc), 0, 0);
        X509_free(cc);
    }else KLOG(ERR) << "Client does not have certificate.";

    KOUT(DBG) << "New connection , socket fd is " << newsockfd << ", is : " << inet_ntoa(cli_addr.sin_addr) << ", port : "<< ntohs(cli_addr.sin_port);
    onConnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    int16_t e;
    char buffer[_KUL_HTTPS_READ_BUFFER_];
    std::stringstream cnt;
    do{
        bzero(buffer,_KUL_HTTPS_READ_BUFFER_);
        e = SSL_read(ssl, buffer, _KUL_HTTPS_READ_BUFFER_ - 1);
        if(e) cnt << buffer;
    }while(e == (_KUL_HTTPS_READ_BUFFER_ - 1));
    if (e < 0){ 
        short se = 0;
        SSL_get_error(ssl, se);
        if(se) KLOG(ERR) << "SSL_get_error: " << se;
        e = -1;
    }else
        try{
            std::string res;
            std::shared_ptr<kul::http::ARequest> req = handleRequest(cnt.str(), res);
            const kul::http::AResponse& rs(response(res, *req.get()));
            std::string ret;
            rs.toString(ret);
            e = SSL_write(ssl, ret.c_str(), ret.length());
        }catch(const kul::http::Exception& e1){
            KERR << e1.what(); 
            e = -1;
        }
    close(newsockfd);
    KOUT(DBG) << "Disconnect , socket fd is " << newsockfd << ", is : " << inet_ntoa(cli_addr.sin_addr) << ", port : "<< ntohs(cli_addr.sin_port);
    onDisconnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
}

void kul::https::Server::setChain(const kul::File& f){
    if(!f) KEXCEPTION("HTTPS Server chain file does not exist: " + f.full());
    if(SSL_CTX_use_certificate_chain_file(ctx, f.mini().c_str()) <= 0)
        KEXCEPTION("HTTPS Server SSL_CTX_use_PrivateKey_file failed");
}

kul::https::Server& kul::https::Server::init(){
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
    s = 0;
    ERR_free_strings();
    EVP_cleanup();
    if(ssl){
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if(ctx) SSL_CTX_free(ctx);
    kul::http::Server::stop();
}

#endif//_KUL_HTTPS_