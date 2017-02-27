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
#ifndef _KUL_HTTP_HPP_
#define _KUL_HTTP_HPP_

#include "kul/tcp.hpp"
#include "kul/http/def.hpp"
#include "kul/http.base.hpp"
#include "kul/threads.hpp"

namespace kul{ namespace http{

class Server : public kul::http::AServer{
    protected:
        virtual std::shared_ptr<ARequest> handleRequest(const std::string& b, std::string& path);
        virtual void receive(const uint16_t& fd, int16_t i = -1) override;

    public:
        Server(const short& p = 80, const std::string& w = "localhost") : AServer(p){}
        
};

class MultiServer : public kul::http::Server{
    protected:
        uint16_t _threads;
        ChroncurrentThreadPool<> _pool;

        void operate(){
            while(s) loop();
        }
        virtual void error(const kul::Exception& e){ 
            KERR << e.stack(); 
            _pool.async(std::bind(&MultiServer::operate, std::ref(*this)), 
                    std::bind(&MultiServer::error, std::ref(*this), std::placeholders::_1));
        };
    public:
        MultiServer(const short& p = 80, const uint16_t& threads = 1, const std::string& w = "localhost") 
                : Server(p, w), _threads(threads), _pool(threads){

        }
        virtual void start() throw (kul::tcp::Exception) override {
            KUL_DBG_FUNC_ENTER
            _started = kul::Now::MILLIS();
            listen(sockfd, 5);
            clilen = sizeof(cli_addr);
            s = true;
            _pool.start();
            for(size_t i = 0; i < _threads; i++)
                _pool.async(std::bind(&MultiServer::operate, std::ref(*this)), 
                    std::bind(&MultiServer::error, std::ref(*this), std::placeholders::_1));
        }
        virtual void join(){
            _pool.join();
        }
        virtual void stop() override {
            KUL_DBG_FUNC_ENTER
            _pool.stop();
            for(size_t i = 0; i < _threads; i++) kul::tcp::SocketServer<char>::stop();
        }
        virtual void interrupt(){
            KUL_DBG_FUNC_ENTER
            _pool.interrupt();
        }
};

}}

#endif /* _KUL_HTTP_HPP_ */
