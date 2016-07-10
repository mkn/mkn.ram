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
#ifndef _KUL_TCP_BASE_HPP_
#define _KUL_TCP_BASE_HPP_

namespace kul{ namespace tcp {

class Exception : public kul::Exception{
    public:
        Exception(const char*f, const uint16_t& l, const std::string& s) : kul::Exception(f, l, s){}
};

template <class T = uint8_t>
class ASocket{
	protected:
		bool open = 0;
	public:
		virtual ~ASocket(){}
		virtual bool connect(const std::string& host, const int16_t& port)  = 0;
		virtual bool close()    = 0;
		virtual bool read(T* data, const uint16_t& len) = 0;
		virtual bool write(const T* data, const size_t& len) = 0;

};

template <class T = uint8_t>
class ASocketServer{
	protected:
        uint16_t p;
        uint64_t s;

        ASocketServer(const uint16_t& p) : p(p){}
	public:
		virtual ~ASocketServer(){}
		virtual void start() throw (kul::tcp::Exception) = 0;
        virtual void onConnect(const char* ip, const uint16_t& port){}
        virtual void onDisconnect(const char* ip, const uint16_t& port){}
        const uint64_t  up()   const { return s - kul::Now::MILLIS(); }
        const uint16_t& port() const { return p; }
        bool started() const { return s; }
};

}// END NAMESPACE tcp
}// END NAMESPACE kul

#endif /* _KUL_HTTP_BASE_HPP_ */