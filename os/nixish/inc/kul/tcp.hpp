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
#ifndef _KUL_TCP_HPP_
#define _KUL_TCP_HPP_

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
#include "kul/tcp/def.hpp"
#include "kul/tcp.base.hpp"

namespace kul{ namespace tcp{

template <class T = uint8_t>
class Socket : public ASocket<T>{
    protected:
        int32_t sck = 0;
    public:
        virtual ~Socket(){
            if(this->open) close();
        }
        virtual bool connect(const std::string& host, const int16_t& port) override {
            if(!SOCKET(sck) || !CONNECT(sck, host, port)) return false;
            this->open = true;
            return true;
        }
        virtual bool close() override {
            bool o1 = this->open;
            if(this->open) {
                ::close(sck);
                this->open = 0;
            }
            return o1;
        }
        virtual uint16_t read(T* data, const uint16_t& len) throw(kul::tcp::Exception) override {
            struct timeval tv;
            fd_set fds;
            int16_t d = 0, iof = -1;
            bool r = 0;
            FD_ZERO(&fds);
            FD_SET(sck, &fds);
            tv.tv_sec = 1;
            tv.tv_usec = 500;
            d = select(sck+1, &fds, NULL, NULL, &tv);
            if(d < 0) KEXCEPTION("Failed to read from Server socket");
            else 
            if(d > 0 && FD_ISSET(sck, &fds)) {
              if ((iof = fcntl(sck, F_GETFL, 0)) != -1) fcntl(sck, F_SETFL, iof | O_NONBLOCK);
              d = recv(sck, data, len, 0);
              if (iof != -1) fcntl(sck, F_SETFL, iof);
            }else if(d == 0 && !FD_ISSET(sck, &fds)){
                d = recv(sck, data, len, 0);
                if(!r && !d) KEXCEPTION("Failed to read from Server socket");
            }
            return d;
        }
        virtual int16_t write(const T* data, const uint16_t& len) override {
            return ::send(sck, data, len, 0);
        }

        static bool SOCKET(int32_t& sck,
            const int16_t& domain = AF_INET, 
            const int16_t& type = SOCK_STREAM, 
            const int16_t& protocol = IPPROTO_TCP){

            sck = socket(domain, type, protocol);
            if(sck < 0) KLOG(DBG) << "SOCKET ERROR CODE: " << sck;
            return sck >= 0; 
        }
        static bool CONNECT(const int32_t& sck, const std::string& host, const int16_t& port){
            struct sockaddr_in servAddr;
            memset(&servAddr, 0, sizeof(servAddr));
            servAddr.sin_family   = AF_INET;
            int16_t e = 0;
            servAddr.sin_port = !kul::byte::isBigEndian() ? htons(port) : kul::byte::LittleEndian::UINT32(port);
            if(host == "localhost" || host == "127.0.0.1"){   
                servAddr.sin_addr.s_addr = INADDR_ANY;
                e = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr));
                if(e < 0) KLOG(DBG) << "SOCKET CONNECT ERROR CODE: " << e << " errno: " << errno;
            }
            else if(inet_pton(AF_INET, &host[0], &(servAddr.sin_addr))){
                e = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr));
                if(e < 0) KLOG(DBG) << "SOCKET CONNECT ERROR CODE: " << e << " errno: " << errno;
            }
            else{
                std::string ip;
                struct addrinfo hints, *servinfo, *next;
                struct sockaddr_in *in;
                memset(&hints, 0, sizeof hints);
                hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
                hints.ai_socktype = SOCK_STREAM;
                if ((e = getaddrinfo(host.c_str(), 0, &hints, &servinfo)) != 0) 
                    KEXCEPTION("getaddrinfo failed for host: " + host);
                for(next = servinfo; next != NULL; next = next->ai_next){
                    in = (struct sockaddr_in *) next->ai_addr;
                    ip = inet_ntoa(in->sin_addr);
                    servAddr.sin_addr.s_addr = inet_addr(&ip[0]);
                    if(ip == "0.0.0.0") continue;
                    if((e = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr))) == 0) break;
                }
                freeaddrinfo(servinfo);
                if(e < 0) KLOG(DBG) << "SOCKET CONNECT ERROR CODE: " << e << " errno: " << errno;
            }
            return e >= 0;
        }
};

template <class T = uint8_t>
class SocketServer : public ASocketServer<T>{
    protected:
        bool s = 0;
        uint16_t clients[_KUL_TCP_MAX_CLIENT_] = {0};
        fd_set bfds, fds;
        int32_t sockfd, newsockfd;
        int16_t max_fd = 0, sd = 0;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        virtual bool handle(T* in, T* out){
            return true;
        }
        virtual void receive(const uint16_t& fd, int16_t i = -1){
            T in[_KUL_TCP_READ_BUFFER_];
            bzero(in, _KUL_TCP_READ_BUFFER_);
            int16_t e = 0, read = ::read(fd, in, _KUL_TCP_READ_BUFFER_ - 1);
            if(read == 0){
                getpeername(fd , (struct sockaddr*) &cli_addr , (socklen_t*)&clilen);
                KOUT(DBG) << "Host disconnected , ip: " << inet_ntoa(serv_addr.sin_addr) << ", port " << ntohs(serv_addr.sin_port);
                this->onDisconnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                close(fd);
                close(i);
                FD_CLR(fd, &bfds);
                if(i > 0) clients[i] = 0;
            }else{
                bool cl = 1;
                in[read] = '\0';
                try{
                    T out[_KUL_TCP_READ_BUFFER_];
                    bzero(out, _KUL_TCP_READ_BUFFER_);
                    cl = handle(in, out);
                    e = ::send(fd, out, strlen(out), 0);
                }catch(const kul::tcp::Exception& e1){
                    KERR << e1.stack(); 
                    e = -1;
                }
                if(e < 0){
                    KLOG(ERR) << "Error replying to host error: " << e;
                    KLOG(ERR) << "Error replying to host errno: " << errno;
                }
                if(e < 0 || cl){
                    close(fd);
                    if(i > 0) clients[i] = 0;
                    FD_CLR(fd, &bfds);   
                }
            }
        }
        virtual void loop() throw(kul::tcp::Exception){
            fds = bfds;

            FD_ZERO(&fds);
            FD_SET(sockfd, &fds);
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 500;

            if(select(_KUL_TCP_MAX_CLIENT_, &fds, NULL, NULL, &tv) < 0 && errno != EINTR) 
                KEXCEPTION("Socket Server error on select");

            if(FD_ISSET(sockfd, &fds)){
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                if(newsockfd < 0) KEXCEPTION("SockerServer error on accept");
              
                KOUT(DBG) << "New connection , socket fd is " << newsockfd << ", is : " << inet_ntoa(cli_addr.sin_addr) << ", port : "<< ntohs(cli_addr.sin_port);

                this->onConnect(inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                receive(newsockfd);
                FD_SET(newsockfd, &bfds);
                for(uint16_t i = 0; i < _KUL_TCP_MAX_CLIENT_; i++)
                    if(clients[i] == 0){
                        clients[i] = newsockfd;
                        break;
                    }
            }
            else
                for(uint16_t i = 0; i < _KUL_TCP_MAX_CLIENT_; i++)
                    if(FD_ISSET(clients[i], &fds)){
                        receive(clients[i], i);
                        break;
                    }
        }
        SocketServer(const uint16_t& p) : kul::tcp::ASocketServer<T>(p){}
    public:
        virtual void start() throw (kul::tcp::Exception){
            s = kul::Now::MILLIS();
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            int iso = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iso, sizeof(iso));
            if (sockfd < 0) KEXCEPTION("Socket Server error opening socket");
            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = !kul::byte::isBigEndian() ? htons(this->port()) : kul::byte::LittleEndian::UINT32(this->port());
            int16_t e = 0;
            if ((e = bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) < 0)
                KEXCEPTION("Socket Server error on binding, errno: " + std::to_string(errno));
            listen(sockfd, 5);
            clilen = sizeof(cli_addr);
            s = true;
            while(s) loop();
        }
        virtual void stop(){
            s = 0;
            close(sockfd);
            for(uint16_t i = 0; i < _KUL_TCP_MAX_CLIENT_; i++) shutdown(clients[i], SHUT_RDWR);
        }
};

}// END NAMESPACE tcp
}// END NAMESPACE kul

#endif//_KUL_TCP_HPP_
