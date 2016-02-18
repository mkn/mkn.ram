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

#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> 
#include <arpa/inet.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include "kul/log.hpp"
#include "kul/byte.hpp"
#include "kul/time.hpp"
#include "kul/http/def.hpp"
#include "kul/http.base.hpp"

namespace kul{ namespace http{

class Requester{
    public:
        static void send(const std::string& h, const std::string& req, const uint16_t& p, std::stringstream& ss){
            int32_t sck = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
            ::send(sck, req.c_str(), req.size(), 0); 
            char buffer[_KUL_HTTP_REQUEST_BUFFER_];
            struct timeval tv;
            fd_set fds;
            int iof = -1;
            bool r = 0;
            do{
                uint16_t d = 0;
                FD_ZERO(&fds);
                FD_SET(sck, &fds);
                tv.tv_sec = 1;
                tv.tv_usec = 500;
                e = select(sck+1, &fds, NULL, NULL, &tv);
                if(e < 0) KEXCEPTION("Failed to read from Server socket");
                else 
                if(e > 0 && FD_ISSET(sck, &fds)) {
                  if ((iof = fcntl(sck, F_GETFL, 0)) != -1) fcntl(sck, F_SETFL, iof | O_NONBLOCK);
                  d = recv(sck, buffer, _KUL_HTTP_REQUEST_BUFFER_, 0);
                  for(uint16_t i = 0; i < d; i++) ss << buffer[i];
                  if (iof != -1) fcntl(sck, F_SETFL, iof);
                  r = 1;
                }else if(e == 0 && !FD_ISSET(sck, &fds)){
                    d = recv(sck, buffer, _KUL_HTTP_REQUEST_BUFFER_, 0);
                    if(!r && !d) KEXCEPTION("Failed to read from Server socket");
                    for(uint16_t i = 0; i < d; i++) ss << buffer[i];
                    r = 1;
                }
                if (d == 0 || (d < _KUL_HTTP_REQUEST_BUFFER_ && r)) break;
                r = 1;
            }while(1);
            ::close(sck);
        }
};

class Server : public kul::http::AServer{
    private:
        void receive(const uint16_t& fd, int16_t i = -1){
            char buffer[_KUL_HTTP_READ_BUFFER_];
            bzero(buffer, _KUL_HTTP_READ_BUFFER_);
            int16_t e = 0, read = ::read(fd, buffer, _KUL_HTTP_READ_BUFFER_ - 1);
            if(read == 0){
                getpeername(fd , (struct sockaddr*) &cli_addr , (socklen_t*)&clilen);
                KOUT(DBG) << "Host disconnected , ip: " << inet_ntoa(serv_addr.sin_addr) << ", port " << ntohs(serv_addr.sin_port);
                close(fd);
                close(i);
                FD_CLR(fd, &bfds);
                if(i > 0) clients[i] = 0;
            }else{
                buffer[read] = '\0';
                std::string res;
                try{
                    std::string s(buffer);
                    std::string c(s.substr(0, (s.size() > 9) ? 10 : s.size()));
                    std::vector<char> allowed = {'G', 'P', '/', 'H'};
                    bool f = 0;
                    for(const auto& ch : allowed){
                        f = c.find(ch) != std::string::npos;
                        if(f) break;
                    }
                    if(!f) KEXCEPTION("Logic error encountered, probably https attempt on http port");
                    
                    std::shared_ptr<ARequest> req = handle(s, res);
                    const AResponse& rs(response(res, *req.get()));
                    std::stringstream ss;
                    ss << rs.version() << " " << rs.status() << " " << rs.reason() << kul::os::EOL();
                    for(const auto& h : rs.headers()) ss << h.first << ": " << h.second << kul::os::EOL();
                    for(const auto& p : rs.cookies()){
                        ss << "Set-Cookie: " << p.first << "=" << p.second.value() << "; ";
                        if(p.second.path().size()) ss << "path=" << p.second.path() << "; ";
                        if(p.second.httpOnly()) ss << "httponly; ";
                        if(p.second.secure()) ss << "secure; ";
                        ss << kul::os::EOL();
                    }
                    ss << kul::os::EOL() << rs.body() << "\r\n" << '\0';
                    const std::string& ret(ss.str());
                    e = ::send(fd, ret.c_str(), ret.length(), 0);

                }catch(const kul::http::Exception& e1){
                    KERR << e1.stack(); 
                    e = -1;
                }
                if(e < 0){
                    close(fd);
                    if(i > 0) clients[i] = 0;
                    FD_CLR(fd, &bfds);
                    KLOG(ERR) << "Error replying to host error: " << e;
                    KLOG(ERR) << "Error replying to host errno: " << errno;
                }
            }
        }
    protected:
        bool s = 0;
        uint16_t clients[_KUL_HTTP_MAX_CLIENT_] = {0};
        fd_set bfds, fds;
        int32_t sockfd, newsockfd;
        int16_t max_fd = 0, sd = 0;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        virtual const std::shared_ptr<ARequest> handle(const std::string& b, std::string& res){
            std::string a;
            std::shared_ptr<ARequest> req;
            {
                std::stringstream ss(b);
                {
                    std::string r;
                    std::string l;
                    std::getline(ss, r);
                    std::vector<std::string> lines = kul::String::lines(b); 
                    std::vector<std::string> l0; 
                    kul::String::split(r, ' ', l0);
                    if(!l0.size()) KEXCEPTION("Malformed request found: " + b); 
                    std::string s(l0[1]);
                    if(l0[0] == "GET"){
                        req = get();
                        if(s.find("?") != std::string::npos){
                            a = s.substr(s.find("?") + 1);
                            s = s.substr(0, s.find("?"));
                        }
                    }else 
                    if(l0[0] == "POST") req = post();
                    else KEXCEPTION("HTTP Server request type not handled: " + l0[0]); 
                    res = s;
                }
                {
                    std::string l;
                    while(std::getline(ss, l)){
                        if(l.size() <= 1) break;
                        std::vector<std::string> bits;
                        kul::String::split(l, ':', bits);
                        kul::String::trim(bits[0]);
                        std::stringstream sv;
                        if(bits.size() > 1) sv << bits[1];
                        for(size_t i = 2; i < bits.size(); i++) sv << ":" << bits[i];
                        std::string v(sv.str());
                        kul::String::trim(v);
                        if(*v.rbegin() == '\r') v.pop_back();
                        if(bits[0] == "Cookie"){
                            for(const auto& coo : kul::String::split(v, ';')){
                                if(coo.find("=") == std::string::npos) req->cookie(coo, "");
                                else{
                                    std::vector<std::string> kv;
                                    kul::String::escSplit(coo, '=', kv);
                                    req->cookie(kv[0], kv[1]);
                                }
                            }
                        }
                        else
                            req->header(bits[0], v);
                    }
                    std::stringstream ss1;
                    while(std::getline(ss, l)) ss1 << l;
                    if(a.empty()) a = ss1.str();
                    else req->body(ss1.str());
                }
            }
            kul::hash::map::S2S atts;
            asAttributes(a, atts);
            for(const auto& att : atts) req->attribute(att.first, att.second);
            return req;
        }
        virtual void loop() throw(kul::http::Exception){
            fds = bfds;

            FD_ZERO(&fds);
            FD_SET(sockfd, &fds);
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 500;

            if(select(_KUL_HTTP_MAX_CLIENT_, &fds, NULL, NULL, &tv) < 0 && errno != EINTR) 
                KEXCEPTION("HTTP Server error on select");

            if(FD_ISSET(sockfd, &fds)){
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                if(newsockfd < 0) KEXCEPTION("HTTP Server error on accept");
              
                printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , 
                    newsockfd , inet_ntoa(cli_addr.sin_addr) , ntohs(cli_addr.sin_port));
                receive(newsockfd);
                FD_SET(newsockfd, &bfds);
                for(uint16_t i = 0; i < _KUL_HTTP_MAX_CLIENT_; i++)
                    if(clients[i] == 0){
                        clients[i] = newsockfd;
                        break;
                    }
            }
            else
                for(uint16_t i = 0; i < _KUL_HTTP_MAX_CLIENT_; i++)
                    if(FD_ISSET(clients[i], &fds)){
                        receive(clients[i], i);
                        break;
                    }
        }

        virtual AResponse& response(AResponse& r) const {
            if(!r.header("Date"))           r.header("Date", kul::DateTime::NOW());
            if(!r.header("Connection"))     r.header("Connection", "close");
            if(!r.header("Content-Type"))   r.header("Content-Type", "text/html");
            if(!r.header("Content-Length")) r.header("Content-Length", std::to_string(r.body().size()));
            return r; 
        }
    public:
        Server(const short& p = 80, const std::string& w = "localhost") : AServer(p){}
        virtual void start() throw(kul::http::Exception){
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            int iso = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iso, sizeof(iso));
            if (sockfd < 0) KEXCEPTION("HTTP Server error opening socket");
            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = !kul::byte::isBigEndian() ? htons(this->port()) : kul::byte::LittleEndian::UINT32(this->port());
            int16_t e = 0;
            if ((e = bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) < 0)
                KEXCEPTION("HTTP Server error on binding, errno: " + std::to_string(errno));
            listen(sockfd, 5);
            clilen = sizeof(cli_addr);
            s = 1;
            while(s) loop();
        }
        bool started() const { return s; }
        virtual void stop(){
            close(sockfd);
            for(uint16_t i = 0; i < _KUL_HTTP_MAX_CLIENT_; i++) shutdown(clients[i], SHUT_RDWR);
            s = false;
        }
        virtual const AResponse response(const std::string& res, const ARequest& req){
            _1_1Response r;
            return response(r);
        }
};
}}

inline void kul::http::_1_1GetRequest::send(const std::string& h, const std::string& res, const uint16_t& p){
    try{
        std::stringstream ss;
        Requester::send(h, toString(h, res), p, ss);
        handle(ss.str());
    }catch(const kul::Exception& e){
        KEXCEPT(Exception, "HTTP GET failed with host: " + h);
    }
}

inline void kul::http::_1_1PostRequest::send(const std::string& h, const std::string& res, const uint16_t& p){
    try{
        std::stringstream ss;
        Requester::send(h, toString(h, res), p, ss);
        handle(ss.str());
    }catch(const kul::Exception& e){
        KEXCEPT(Exception, "HTTP POST failed with host: " + h);
    }
}


#endif /* _KUL_HTTP_HPP_ */
