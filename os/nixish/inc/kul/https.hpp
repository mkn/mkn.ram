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
#ifndef _KUL_HTTPS_HPP_
#define _KUL_HTTPS_HPP_

#include <openssl/ssl.h> 
#include <openssl/err.h> 

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#include "kul/http.hpp"

namespace kul{ namespace https{

class Server : public kul::http::Server{
    protected:
        X509* cc = {0};
        SSL *ssl = {0};
        SSL_CTX *ctx = {0};
        kul::File crt, key;
        virtual void loop() throw(kul::http::Exception){
            int32_t newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            if(newsockfd < 0) KEXCEPTION("HTTPS Server error on accept");

            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, newsockfd);
            //Here is the SSL Accept portion.  Now all reads and writes must use SSL
            int16_t ssl_err = SSL_accept(ssl);
            if(ssl_err <= 0) KEXCEPTION("HTTPS Server SSL ERROR on SSL_ACCEPT error: " + std::to_string(ssl_err ));

            KLOG(DBG) << "SSL_get_cipher: " << SSL_get_cipher(ssl);
            cc = SSL_get_peer_certificate (ssl);

            if(cc != NULL) {
                KLOG(DBG) << "Client certificate:";
                KLOG(DBG) << "\t subject: " << X509_NAME_oneline (X509_get_subject_name (cc), 0, 0);
                KLOG(DBG) << "\t issuer: %s\n" << X509_NAME_oneline (X509_get_issuer_name  (cc), 0, 0);
                X509_free(cc);
            }else KLOG(ERR) << "Client does not have certificate.";

            int16_t e;
            char buffer[_KUL_HTTPS_READ_BUFFER_];
            bzero(buffer,_KUL_HTTPS_READ_BUFFER_);
            e = SSL_read(ssl, buffer, _KUL_HTTPS_READ_BUFFER_ - 1);
            if (e <= 0){ 
                short se = 0;
                SSL_get_error(ssl, se);
                if(se) KLOG(ERR) << "SSL_get_error: " << se;
                e = -1;
            }else
                try{
                    std::string res;
                    std::shared_ptr<kul::http::ARequest> req = handle(std::string(buffer), res);
                    const kul::http::AResponse& rs(response(res, *req.get()));
                    std::stringstream ss;
                    ss << rs.version() << " " << rs.status() << " " << rs.reason() << kul::os::EOL();
                    for(const auto& h : rs.headers()) ss << h.first << ": " << h.second << kul::os::EOL();
                    for(const auto& p : rs.cookies()){
                        ss << "Set-Cookie: " << p.first << "=" << p.second.value() << "; ";
                        if(p.second.domain().size()) ss << "domain=" << p.second.domain() << "; ";
                        if(p.second.path().size()) ss << "path=" << p.second.path() << "; ";
                        if(p.second.httpOnly()) ss << "httponly; ";
                        if(p.second.secure()) ss << "secure; ";
                        ss << kul::os::EOL();
                    }
                    ss << kul::os::EOL() << rs.body();;
                    const std::string& ret(ss.str());
                    e = SSL_write(ssl, ret.c_str(), ret.length());
                }catch(const kul::http::Exception& e1){
                    KERR << e1.what(); 
                    e = -1;
                }
            close(newsockfd);
        }
    public:
        Server(const kul::File& c, const kul::File& k, const std::string& w = "localhost") : kul::http::Server(443), crt(c), key(k){}
        Server(const short& p, const kul::File& c, const kul::File& k, const std::string& w = "localhost") : kul::http::Server(p), crt(c), key(k){}
        ~Server(){
            if(s) stop();
        }
        void start() throw(kul::http::Exception){
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
            kul::http::Server::start();
        }
        void stop(){
            ERR_free_strings();
            EVP_cleanup();
            if(ssl){
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            if(ctx) SSL_CTX_free(ctx);
            kul::http::Server::stop();
        }
};

class ARequest;
class Requester;
class SSLReqHelper{
    private:
        SSL_CTX *ctx;
        SSLReqHelper(){
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
            ctx = SSL_CTX_new(TLSv1_2_client_method());
            if ( ctx == NULL ) {
                ERR_print_errors_fp(stderr);
                abort();
                KEXCEPTION("HTTPS Request SSL_CTX FAILED");
            }
        }
        ~SSLReqHelper(){
            SSL_CTX_free(ctx);
        }
        static SSLReqHelper& INSTANCE(){
            static SSLReqHelper i;
            return i;
        }
        friend class ARequest;
        friend class Requester;

};

class ARequest{
    protected:
        SSL *ssl = {0};
        ARequest() : ssl(SSL_new(SSLReqHelper::INSTANCE().ctx)){}
        ~ARequest(){
            SSL_free(ssl);
        }
};

class Requester{
    public:
        static void send(const std::string& h, const std::string& req, const uint16_t& p, std::stringstream& ss, SSL *ssl){
            // int32_t sck = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            int32_t sck = socket(PF_INET, SOCK_STREAM, 0);
            if (sck < 0) KEXCEPT(kul::http::Exception, "Error opening socket");
            struct sockaddr_in servAddr;
            memset(&servAddr, 0, sizeof(servAddr));
            servAddr.sin_family   = AF_INET;
            int e = 0;
            servAddr.sin_port = !kul::byte::isBigEndian() ? htons(p) : kul::byte::LittleEndian::UINT32(p);
            if(h == "localhost" || h == "127.0.0.1"){   
                servAddr.sin_addr.s_addr = INADDR_ANY;
                e = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr));
            }
            else if(inet_pton(AF_INET, &h[0], &(servAddr.sin_addr))){
                e = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr));
            }
            else{
                std::string ip;
                struct addrinfo hints, *servinfo, *next;
                struct sockaddr_in *in;
                memset(&hints, 0, sizeof hints);
                hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
                hints.ai_socktype = SOCK_STREAM;
                if ((e = getaddrinfo(h.c_str(), 0, &hints, &servinfo)) != 0) 
                    KEXCEPT(kul::http::Exception, "getaddrinfo failed for host: " + h);
                for(next = servinfo; next != NULL; next = next->ai_next){
                    in = (struct sockaddr_in *) next->ai_addr;
                    ip = inet_ntoa(in->sin_addr);
                    servAddr.sin_addr.s_addr = inet_addr(&ip[0]);
                    if(ip == "0.0.0.0") continue;
                    if((e = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr))) == 0) break;
                }
                freeaddrinfo(servinfo);
            }
            if(e < 0) KEXCEPT(kul::http::Exception, "Failed to connect to host: " + h);
            SSL_set_fd(ssl, sck);
            if (SSL_connect(ssl) == -1) KEXCEPTION("HTTPS REQUEST INIT FAILED");
            SSL_write(ssl, req.c_str(), req.size());
            char buffer[_KUL_HTTPS_REQUEST_BUFFER_];
            do{
                int16_t d = SSL_read(ssl, buffer, _KUL_HTTPS_REQUEST_BUFFER_ - 1);
                if (d == 0) break; 
                if (d < 0){ 
                    short se = 0;
                    SSL_get_error(ssl, se);
                    if(se) KLOG(ERR) << "SSL_get_error: " << se;
                    break;
                }
                for(uint16_t i = 0; i < d; i++) ss << buffer[i];
            }while(1);
            ::close(sck);
        }
};

class _1_1GetRequest : public http::_1_1GetRequest, https::ARequest{
    public:
        void send(const std::string& h, const std::string& res, const uint16_t& p = 443){
            try{
                std::stringstream ss;
                KLOG(DBG) << toString(h, res);
                Requester::send(h, toString(h, res), p, ss, ssl);
                handle(ss.str());
            }catch(const kul::Exception& e){
                KEXCEPT(Exception, "HTTP GET failed with host: " + h);
            }
        }
};

class _1_1PostRequest : public http::_1_1PostRequest, https::ARequest{
    public:
        void send(const std::string& h, const std::string& res, const uint16_t& p = 443){
            try{
                std::stringstream ss;
                KLOG(DBG) << toString(h, res);
                Requester::send(h, toString(h, res), p, ss, ssl);
                handle(ss.str());
            }catch(const kul::Exception& e){
                KEXCEPT(Exception, "HTTP POST failed with host: " + h);
            }
        }
};

}}
#endif /* _KUL_HTTPS_HPP_ */
