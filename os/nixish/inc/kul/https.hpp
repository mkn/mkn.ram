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
        SSL * ssl_clients[_KUL_TCP_MAX_CLIENT_] = {0};
        SSL_CTX *ctx = {0};
        kul::File crt, key;
        const std::string cs;
        virtual void loop(std::unordered_set<int>& fds) throw(kul::tcp::Exception) override;
        virtual bool receive(const int& fd) override;
    public:
        Server(const kul::File& c, const kul::File& k, const std::string& cs = "") : kul::http::Server(443), crt(c), key(k), cs(cs){}
        Server(const short& p, const kul::File& c, const kul::File& k, const std::string& cs = "") : kul::http::Server(p), crt(c), key(k), cs(cs){}
        virtual ~Server(){
            if(s) stop();
        }
        void setChain(const kul::File& f);
        Server& init();
        virtual void stop() override;
};

class MultiServer : public kul::https::Server{
    protected:
        uint16_t _threads;
        kul::Mutex m_mutAccept, m_mutSelect, m_mutFDMod;
        ChroncurrentThreadPool<> _pool;

        void operate(){
            std::unordered_set<int> fds;
            while(s) loop(fds);
        }
        virtual void error(const kul::Exception& e){ 
            KERR << e.stack(); 
            _pool.async(std::bind(&MultiServer::operate, std::ref(*this)), 
                    std::bind(&MultiServer::error, std::ref(*this), std::placeholders::_1));
        };
    public:
        MultiServer(const uint16_t& threads, const kul::File& c, const kul::File& k, const std::string& cs = "")
                : kul::https::Server(c, k, cs), _threads(threads){}
        MultiServer(const short& p, const uint16_t& threads, const kul::File& c, const kul::File& k, const std::string& cs = "")
                : kul::https::Server(p, c, k, cs), _threads(threads){}

        virtual ~MultiServer(){
            _pool.stop();
        }

        virtual void start() throw (kul::tcp::Exception) override;

        virtual void join(){
            _pool.join();
        }
        virtual void stop() override {
            KUL_DBG_FUNC_ENTER
            _pool.stop();
            kul::https::Server::stop();
        }
        virtual void interrupt(){
            _pool.interrupt();
        }
        virtual void copyFDSet(fd_set& from, fd_set& to) override {
            kul::ScopeLock lock(m_mutFDMod);
            kul::tcp::SocketServer<char>::copyFDSet(from, to);
        }
        virtual int select(const int& max, fd_set* set, struct timeval* tv) override {
            kul::ScopeLock lock(m_mutSelect);
            return ::select(max, set, NULL, NULL, tv);
        }
        virtual int accept() override {
            kul::ScopeLock lock(m_mutAccept);
            return ::accept(lisock, (struct sockaddr *) &cli_addr, &clilen);
        }
};

class ARequest;
class Requester;
class SSLReqHelper{
    friend class ARequest;
    friend class Requester;
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
        static void send(const std::string& h, const std::string& req, const uint16_t& p, std::stringstream& ss, SSL *ssl);
};

class _1_1GetRequest : public http::_1_1GetRequest, https::ARequest{
    public:
        _1_1GetRequest(const std::string& host, const std::string& path = "", const uint16_t& port = 443) 
            : http::_1_1GetRequest(host, path, port){}
        virtual void send() throw (kul::http::Exception) override;
};

class _1_1PostRequest : public http::_1_1PostRequest, https::ARequest{
    public:
        _1_1PostRequest(const std::string& host, const std::string& path = "", const uint16_t& port = 443) 
            : http::_1_1PostRequest(host, path, port){}
        virtual void send() throw (kul::http::Exception) override;
};

}}
#endif//_KUL_HTTPS_HPP_
