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

#include <unordered_set>
#include <map>

#include "kul/tcp/def.hpp"

#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "kul/tcp.base.hpp"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

namespace kul{ namespace tcp{

template <class T = uint8_t>
class Socket : public ASocket<T>{
    protected:
        int iResult;

        WSADATA wsaData;
        SOCKET ConnectSocket = INVALID_SOCKET;
        struct addrinfo *result = NULL,
                        *ptr = NULL,
                        hints;
    public:
        virtual ~Socket(){
            if(this->open) close();
        }
        virtual bool connect(const std::string& host, const int16_t& port) override {
            KUL_DBG_FUNC_ENTER
            if(!CONNECT(*this, host, port)) return false;
            this->open = true;
            return true;
        }
        virtual bool close() override {
            KUL_DBG_FUNC_ENTER
            bool o1 = this->open;
            if(this->open){
                this->open = 0;
                if(ConnectSocket == INVALID_SOCKET)
                    KERR << "Socket shutdown: Socket is invalid ignoring";
                else{
                    closesocket(ConnectSocket);
                    WSACleanup();
                }
            }
            return o1;
        }
        virtual size_t read(T* data, const size_t& len) KTHROW(kul::tcp::Exception) override {
            KUL_DBG_FUNC_ENTER

            int16_t d = recv(ConnectSocket, data, len, 0);

            return d;
        }
        virtual size_t write(const T* data, const size_t& len) override {
            iResult = send( ConnectSocket, data, len, 0 );
            if (iResult == SOCKET_ERROR)
                KEXCEPTION("Socket send failed with error: ") << WSAGetLastError();
            return iResult;
        }

    protected:
        static bool CONNECT(Socket& sck, const std::string& host, const int16_t& port){
            KUL_DBG_FUNC_ENTER
            int16_t e = 0;

            // Initialize Winsock
            int iResult = WSAStartup(MAKEWORD(2,2), &sck.wsaData);
            if (iResult != 0) {
                KEXCEPTION("WSAStartup failed with error: ") << iResult;
                return 1;
            }

            ZeroMemory( &sck.hints, sizeof(sck.hints) );
            sck.hints.ai_family = AF_UNSPEC;
            sck.hints.ai_socktype = SOCK_STREAM;
            sck.hints.ai_protocol = IPPROTO_TCP;

            // Resolve the server address and port
            iResult = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &sck.hints, &sck.result);
            if ( iResult != 0 )
                KEXCEPTION("getaddrinfo failed with error: ") << iResult;

            // Attempt to connect to an address until one succeeds
            for(sck.ptr=sck.result; sck.ptr != NULL ; sck.ptr = sck.ptr->ai_next) {

                // Create a SOCKET for connecting to server
                sck.ConnectSocket = 
                    ::socket(sck.ptr->ai_family, sck.ptr->ai_socktype, sck.ptr->ai_protocol);
                if (sck.ConnectSocket == INVALID_SOCKET)
                    KEXCEPTION("socket failed with error: ") << WSAGetLastError();

                // Connect to server.
                iResult = ::connect( sck.ConnectSocket, sck.ptr->ai_addr, (int) sck.ptr->ai_addrlen);
                if (iResult == SOCKET_ERROR) {
                    closesocket(sck.ConnectSocket);
                    sck.ConnectSocket = INVALID_SOCKET;
                    continue;
                }
                break;
            }

            freeaddrinfo(sck.result);

            if (sck.ConnectSocket == INVALID_SOCKET)
                KEXCEPTION("Unable to connect to server!");

            return e >= 0;
        }
};

template <class T = uint8_t>
class SocketServer : public ASocketServer<T>{
    protected:
        bool s = 0;
        int iResult;
        int64_t _started;


        WSADATA wsaData;
        
        SOCKET lisock = INVALID_SOCKET;
        SOCKET ClientSocket = INVALID_SOCKET;

        struct addrinfo *result = NULL;
        struct addrinfo hints;

        virtual bool handle(T* in, T* out){
            return true;
        }
        virtual int readFrom(const int& fd, T* in){
            return ::recv(fd, in, _KUL_TCP_READ_BUFFER_ - 1, 0);
        }
        virtual int writeTo(const int& fd, const T*const out, size_t size){
            return ::send(fd, out, size, 0);
        }

        virtual bool receive(std::map<int, uint8_t>& fds , const int& fd){
            KUL_DBG_FUNC_ENTER
            T in[_KUL_TCP_READ_BUFFER_];
            ZeroMemory(in, _KUL_TCP_READ_BUFFER_);

            do {
                bool cl = 1;
                iResult = recv(ClientSocket, in, _KUL_TCP_READ_BUFFER_ - 1, 0);
                if (iResult > 0) {
                    T out[_KUL_TCP_READ_BUFFER_];
                    ZeroMemory(out, _KUL_TCP_READ_BUFFER_);
                    cl = handle(in, out);
                    auto sent = writeTo( ClientSocket, out, strlen(out));
                    if (sent == SOCKET_ERROR)
                        KEXCEPTION("SocketServer send failed with error: ") << WSAGetLastError();
                }
                else if (iResult == 0){
                    //printf("Connection closing...\n");
                }
                else
                    KEXCEPTION("SocketServer recv failed with error: ") << WSAGetLastError();

            } while (iResult > 0);

            return true;
        }

        virtual void loop(std::map<int, uint8_t>& fds) KTHROW(kul::tcp::Exception){
            ClientSocket = accept();
            if(!s) return;
            if (ClientSocket == INVALID_SOCKET)
                KEXCEPTION("SocketServer accept failed with error: ") << WSAGetLastError();
            receive(fds, ClientSocket);
        }

        virtual SOCKET accept(){
            return ::accept(lisock, NULL, NULL);
        }
        
    public:
        SocketServer(const uint16_t& p, bool _bind = 1) : kul::tcp::ASocketServer<T>(p){
            // if(_bind) bind(__KUL_TCP_BIND_SOCKTOPTS__);
        }
        void freeaddrinfo(){
            if(!result) return;
            ::freeaddrinfo(result);
            result = nullptr;
        }
        ~SocketServer(){
            freeaddrinfo();
            if(lisock) closesocket(lisock);
            if(ClientSocket) closesocket(ClientSocket);
            WSACleanup();
        }
        virtual void bind(int sockOpt = __KUL_TCP_BIND_SOCKTOPTS__) KTHROW(kul::Exception) {

        }
        virtual void start() KTHROW (kul::tcp::Exception){
            KUL_DBG_FUNC_ENTER
            _started = kul::Now::MILLIS();

            iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
            if (iResult != 0) KEXCEPTION("WSAStartup failed with error: ") << iResult;

            ZeroMemory(&hints, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = AI_PASSIVE;

            iResult = getaddrinfo(NULL, std::to_string(port()).c_str(), &hints, &result);
            if ( iResult != 0 ) {
                WSACleanup();
                KEXCEPTION("getaddrinfo failed with error: ") << iResult;
            }

            lisock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            int iso = 1;
            int rc = setsockopt(lisock, SOL_SOCKET, SO_REUSEADDR, (char*)&iso, sizeof(iso));
            if (lisock == INVALID_SOCKET)
                KEXCEPTION("socket failed with error: ") << WSAGetLastError();

            // Setup the TCP listening socket
            iResult = ::bind( lisock, result->ai_addr, (int)result->ai_addrlen);
            if (iResult == SOCKET_ERROR)
                KEXCEPTION("bind failed with error: ") << WSAGetLastError();

            freeaddrinfo();

            iResult = listen(lisock, SOMAXCONN);
            if (iResult == SOCKET_ERROR)
                KEXCEPTION("listen failed with error: ") << WSAGetLastError();

            s = true;
            std::map<int, uint8_t> fds;
            for(int i = 0; i < _KUL_TCP_MAX_CLIENT_; i++) fds.insert(std::make_pair(i, 0));
            try{
                while(s) loop(fds);
            }
            catch(const kul::tcp::Exception& e1){ KERR << e1.stack();  }
            catch(const std::exception& e1)     { KERR << e1.what();   }
            catch(...)                          { KERR << "Loop Exception caught"; }
        }
        virtual void stop(){
            KUL_DBG_FUNC_ENTER
            s = 0;
            ::closesocket(lisock);
            lisock = 0;
            ::closesocket(ClientSocket);
            ClientSocket = 0;
            // for(int i = 0; i < _KUL_TCP_MAX_CLIENT_; i++)
            //     if(i != lisock)
            //         shutdown(i, SHUT_RDWR);
        }
};

}// END NAMESPACE tcp
}// END NAMESPACE kul

#endif//_KUL_TCP_HPP_
