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
#ifdef  _KUL_INCLUDE_HTTPS_
#include "kul/https.hpp"

void
kul::https::Requester::send(const std::string& h, const std::string& req, const uint16_t& p, std::stringstream& ss, SSL *ssl){
    KUL_DBG_FUNC_ENTER
    {
        KLOG(INF);
        kul::tcp::Socket<char> sock;
        KLOG(INF);
        if(!sock.connect(h, p)) KEXCEPTION("TCP FAILED TO CONNECT!");
        KLOG(INF);
        SOCKET sck = sock.socket();
        SSL_set_fd(ssl, sck);
        KLOG(INF);
        if (SSL_connect(ssl) == -1) KEXCEPTION("HTTPS REQUEST INIT FAILED");
        KLOG(INF);
        SSL_write(ssl, req.c_str(), req.size());
        KLOG(INF);
        char buffer[_KUL_TCP_REQUEST_BUFFER_];
        int d = 0;
        uint32_t i;
        do{
        KLOG(INF);
            bzero(buffer, _KUL_TCP_REQUEST_BUFFER_);
        KLOG(INF);
            d = SSL_read(ssl, buffer, _KUL_TCP_REQUEST_BUFFER_ - 1);
        KLOG(INF);
            if (d == 0) break;
            if (d < 0){
                short se = 0;
                SSL_get_error(ssl, se);
                if(se) KLOG(ERR) << "SSL_get_error: " << se;
                break;
            }
            for(i = 0; i < d; i++) ss << buffer[i];
        }while(true);
        KLOG(INF);
        ::closesocket(sck);
        KLOG(INF);
    }
}

void
kul::https::_1_1GetRequest::send() KTHROW (kul::http::Exception){
    KUL_DBG_FUNC_ENTER
    try{
        std::stringstream ss;
        Requester::send(_host, toString(), _port, ss, ssl);

        auto rec(ss.str());
        kul::http::_1_1Response res(kul::http::_1_1Response::FROM_STRING(rec));
        handleResponse(res);
    }catch(const kul::Exception& e){
        KLOG(ERR) << e.debug();
        KEXCEPT(Exception, "HTTP GET failed with host: " + _host);
    }
}

void
kul::https::_1_1PostRequest::send() KTHROW (kul::http::Exception){
    KUL_DBG_FUNC_ENTER
    try{
        std::stringstream ss;
        Requester::send(_host, toString(), _port, ss, ssl);

        auto rec(ss.str());
        kul::http::_1_1Response res(kul::http::_1_1Response::FROM_STRING(rec));
        handleResponse(res);
    }catch(const kul::Exception& e){
        KLOG(ERR) << e.debug();
        KEXCEPT(Exception, "HTTP POST failed with host: " + _host);
    }
}


#endif//_KUL_INCLUDE_HTTPS_