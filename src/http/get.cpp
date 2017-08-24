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
#include "kul/http.base.hpp"

void
kul::http::_1_1GetRequest::send() KTHROW (kul::http::Exception){
    KUL_DBG_FUNC_ENTER
    kul::tcp::Socket<char> sock;
    if(!sock.connect(_host, _port)) KEXCEPTION("TCP FAILED TO CONNECT!");
    const std::string& req(toString());
    sock.write(req.c_str(), req.size());
    char buf[_KUL_TCP_REQUEST_BUFFER_];
    std::stringstream ss;
    size_t s = 0;
    do{
        s = sock.read(buf, _KUL_TCP_REQUEST_BUFFER_ - 1);
        ss << buf;
    }while(s == _KUL_TCP_REQUEST_BUFFER_ - 1);
    auto rec(ss.str());
    _1_1Response res(_1_1Response::FROM_STRING(rec));
    handleResponse(res);
}
