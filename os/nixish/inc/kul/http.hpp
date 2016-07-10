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
#include "kul/tcp.hpp"
#include "kul/byte.hpp"
#include "kul/time.hpp"
#include "kul/http/def.hpp"
#include "kul/http.base.hpp"

namespace kul{ namespace http{

class Server : public kul::http::AServer{
    protected:
        virtual const std::shared_ptr<ARequest> handle(const std::string& b, std::string& res){
             std::string a;
             std::shared_ptr<ARequest> req;
             {
                 std::stringstream ss(b);
                 {
                     std::string r;
                     std::string l;
                     std::getline(ss, r);
                     std::vector<std::string> l0; 
                     kul::String::SPLIT(r, ' ', l0);
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
                         kul::String::SPLIT(l, ':', bits);
                         kul::String::TRIM(bits[0]);
                         std::stringstream sv;
                         if(bits.size() > 1) sv << bits[1];
                         for(size_t i = 2; i < bits.size(); i++) sv << ":" << bits[i];
                         std::string v(sv.str());
                         kul::String::TRIM(v);
                         if(*v.rbegin() == '\r') v.pop_back();
                         if(bits[0] == "Cookie"){
                             for(const auto& coo : kul::String::SPLIT(v, ';')){
                                 if(coo.find("=") == std::string::npos){
                                     req->cookie(coo, "");
                                     KERR << kul::LogMan::INSTANCE().str(__FILE__, __LINE__, kul::log::mode::ERR) 
                                         << "Cookie without equals sign, skipping";
                                 }else{
                                     std::vector<std::string> kv;
                                     kul::String::ESC_SPLIT(coo, '=', kv);
                                     if(kv[1].size()) req->cookie(kv[0], kv[1]);
                                 }
                             }
                         }
                         else
                             req->header(bits[0], v);
                     }
                     std::stringstream ss1;
                     while(std::getline(ss, l)) ss1 << l;
                     if(a.empty()) a = ss1.str();
                     req->body(ss1.str());
                 }
             }
             kul::hash::map::S2S atts;
             asAttributes(a, atts);
             for(const auto& att : atts) req->attribute(att.first, att.second);
             return req;
         }

        virtual void receive(const uint16_t& fd, int16_t i = -1){
            char buffer[_KUL_HTTP_READ_BUFFER_];
            bzero(buffer, _KUL_HTTP_READ_BUFFER_);
            int16_t e = 0, read = ::read(fd, buffer, _KUL_HTTP_READ_BUFFER_ - 1);
            if(read == 0){
                getpeername(fd , (struct sockaddr*) &cli_addr , (socklen_t*)&clilen);
                KOUT(DBG) << "Host disconnected , ip: " << inet_ntoa(serv_addr.sin_addr) << ", port " << ntohs(serv_addr.sin_port);
                onDisconnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
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
                    std::vector<char> allowed = {'D', 'G', 'P', '/', 'H'};
                    bool f = 0;
                    for(const auto& ch : allowed){
                        f = c.find(ch) != std::string::npos;
                        if(f) break;
                    }
                    if(!f) KEXCEPTION("Logic error encountered, probably https attempt on http port");
                    
                    std::shared_ptr<ARequest> req = handle(s, res);
                    const AResponse& rs(response(res, *req.get()));
                    std::string ret;
                    rs.toString(ret);
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
        virtual AResponse& response(AResponse& r) const {
            if(!r.header("Date"))           r.header("Date", kul::DateTime::NOW());
            if(!r.header("Connection"))     r.header("Connection", "close");
            if(!r.header("Content-Type"))   r.header("Content-Type", "text/html");
            if(!r.header("Content-Length")) r.header("Content-Length", std::to_string(r.body().size()));
            return r; 
        }
    public:
        Server(const short& p = 80, const std::string& w = "localhost") : AServer(p){}
        virtual const AResponse response(const std::string& res, const ARequest& req){
            _1_1Response r;
            return response(r);
        }
};
}}

inline void kul::http::_1_1GetRequest::send(const std::string& h, const std::string& res, const uint16_t& p) throw (kul::http::Exception){
    kul::tcp::Socket<char> sock; 
    if(!sock.connect(h, p)) KEXCEPTION("TCP FAILED TO CONNECT!");
    const std::string& req(toString(h, res));
    sock.write(req.c_str(), req.size());
    char buf[_KUL_TCP_REQUEST_BUFFER_];
    sock.read(buf);
    handle(std::string(buf));
}

inline void kul::http::_1_1PostRequest::send(const std::string& h, const std::string& res, const uint16_t& p) throw (kul::http::Exception){
    kul::tcp::Socket<char> sock; 
    if(!sock.connect(h, p)) KEXCEPTION("TCP FAILED TO CONNECT!");
    const std::string& req(toString(h, res));
    sock.write(req.c_str(), req.size());
    char buf[_KUL_TCP_REQUEST_BUFFER_];
    sock.read(buf);
    handle(std::string(buf));
}


#endif /* _KUL_HTTP_HPP_ */
