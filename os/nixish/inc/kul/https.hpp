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
            char buffer[256];
            bzero(buffer,256);
            e = SSL_read(ssl, buffer, 255);
            if (e < 0){ 
                KLOG(ERR) << "ERROR reading from socket"; 
                close(newsockfd); 
                return; 
            }
            std::string b(buffer);
            if(b.empty()){ 
                KLOG(ERR) << "Malformed request found: " << b; 
                close(newsockfd); 
                return; 
            }
            kul::hash::map::S2S hs;
            std::stringstream ss(b);
            std::string l;
            std::string r;
            std::getline(ss, r);
            while(std::getline(ss, l)){
                if(l.size() <= 1) break;
                std::vector<std::string> bits;
                kul::String::split(l, ':', bits);
                hs.insert(bits[0], bits[1]);
            }
            std::vector<std::string> lines = kul::String::lines(b); 
            std::vector<std::string> l0; 
            kul::String::split(r, ' ', l0);
            if(!l0.size()){ KLOG(ERR) << "Malformed request found: " << b; close(newsockfd); return; }
            std::string s(l0[1]);
            std::string a;
            if(l0[0] == "GET"){
                if(s.find("?") != std::string::npos){
                    a = s.substr(s.find("?") + 1);
                    s = s.substr(0, s.find("?"));
                }
            }else 
            if(l0[0] == "POST") while(std::getline(ss, l)) a += l;
            //}else if(l0[0].compare("HEAD") == 0){
            else{ 
                KLOG(ERR) << "HTTPS Server request type not handled: " << l0[0]; 
                close(newsockfd); 
                return; 
            }

            kul::hash::map::S2S atts;
            asAttributes(a, atts);
            const kul::http::AResponse& rs(response(s, hs, atts));
            ss.str(std::string());
            ss << rs.version() << " " << rs.status() << " " << rs.reason() << kul::os::EOL();
            for(const auto& h : rs.headers()) ss << h.first << ": " << h.second << kul::os::EOL();
            ss << kul::os::EOL() << rs.body();;
            const std::string& ret(ss.str());
            e = SSL_write(ssl, ret.c_str(), ret.length());
            if (e < 0) KLOG(ERR) << "Error replying to host";
            close(newsockfd);
        }
    public:
        Server(const short& p, const kul::File& c, const kul::File& k, const std::string& w = "localhost") : kul::http::Server(p), crt(c), key(k){}
        ~Server(){
            if(s){
                stop();
            }
        }
        void start() throw(kul::http::Exception){
            if(!crt) KEXCEPTION("HTTPS Server crt file does not exist: " + crt.full());
            if(!key) KEXCEPTION("HTTPS Server key file does not exist: " + key.full());
            SSL_load_error_strings();
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
        void stop() throw(kul::http::Exception){
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


}}
#endif /* _KUL_HTTPS_HPP_ */