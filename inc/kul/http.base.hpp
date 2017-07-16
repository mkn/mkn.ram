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
#ifndef _KUL_HTTP_BASE_HPP_
#define _KUL_HTTP_BASE_HPP_

#include "kul/map.hpp"
#include "kul/tcp.hpp"
#include "kul/string.hpp"

namespace kul{ namespace http{

typedef kul::hash::map::S2S Headers;

class Exception : public kul::Exception{
    public:
        Exception(const char*f, const uint16_t& l, const std::string& s) : kul::Exception(f, l, s){}
};

class Cookie{
    private:    
        bool h = 0, i = 0, s = 0;
        std::string d, p, v, x;
    public:
        Cookie(const std::string& v) : v(v){}
        Cookie& domain(const std::string& d){ this->d = d; return *this; }
        const std::string& domain() const { return d; }
        Cookie& path(const std::string& p){ this->p = p; return *this; }
        const std::string& expires() const { return x; }
        Cookie& expires(const std::string& x){ this->x = x; return *this; }
        const std::string& path() const { return p; }
        const std::string& value() const { return v; }
        Cookie& httpOnly(bool h){ this->h = h; return *this;  }
        bool httpOnly() const { return h; }
        Cookie& secure(bool s){ this->s = s; return *this; }
        bool secure() const { return s; }
        bool invalidated() const { return i; }
        void invalidate(){
            v = "";
            x = "";
            i = 1;
        }
};

class Message{
    protected:
        Headers _hs;
        std::string _b;
    public:
        void header(const std::string& k, const std::string& v){ this->_hs[k] = v; }
        const Headers& headers() const { return _hs; }
        void body(const std::string& b){ this->_b = b; }
        const std::string& body() const { return _b; }
        bool header(const std::string& s) const {
            return _hs.count(s);
        }
};

class ARequest : public Message{
    protected:
        std::string _host, _path;
        uint16_t _port;
        kul::hash::map::S2S cs;
        kul::hash::map::S2S atts;
        virtual void handleResponse(const kul::hash::map::S2S& h, const std::string& s){}
        void handle(std::string b);
    public:
        ARequest(const std::string& host, const std::string& path, const uint16_t& port) 
            : _host(host), _path(path), _port(port){}
        virtual ~ARequest(){}
        virtual std::string method() const = 0;
        virtual std::string version() const = 0;
        virtual std::string toString() const  = 0;
        void cookie(const std::string& k, const std::string& v) { this->cs.insert(k, v); }
        const kul::hash::map::S2S& cookies() const { return cs; }
        ARequest& attribute(const std::string& k, const std::string& v){
            atts[k] = v;
            return *this;
        }
        const kul::hash::map::S2S&  attributes() const { return atts; }
        virtual void send() KTHROW (kul::http::Exception) = 0;
        const std::string& host() const { return _host; }
        const std::string& path() const { return _path; }
        const uint16_t&    port() const { return _port; }

};

class A1_1Request : public ARequest{
    public:
        A1_1Request(const std::string& host, const std::string& path, const uint16_t& port) 
            : ARequest(host, path, port){}
    protected:
        std::string version() const { return "HTTP/1.1";}
};

class _1_1GetRequest : public A1_1Request{
    public:
        _1_1GetRequest(const std::string& host, const std::string& path = "", const uint16_t& port = 80) 
            : A1_1Request(host, path, port){}
        virtual std::string method() const override { return "GET";}
        virtual std::string toString() const override;
        virtual void send() KTHROW (kul::http::Exception) override;
};

class _1_1PostRequest : public A1_1Request{
    public:
        _1_1PostRequest(const std::string& host, const std::string& path = "", const uint16_t& port = 80) 
            : A1_1Request(host, path, port){}
        virtual std::string method() const override { return "POST";}
        virtual std::string toString() const override;
        virtual void send() KTHROW (kul::http::Exception) override;
};

class AResponse : public Message{
    protected:
        uint16_t _s = 200;
        std::string r = "OK";
        Headers hs;
        kul::hash::map::S2T<Cookie> cs;
    public:
        void cookie(const std::string& k, const Cookie& c){ cs.insert(k, c); }
        const kul::hash::map::S2T<Cookie>& cookies() const { return cs; }
        const std::string& reason() const { return r; }
        void reason(const std::string& r){ this->r = r; }
        const uint16_t& status() const { return _s; }
        void status(const uint16_t& s){ this->_s = s; }
        virtual std::string version() const { return "HTTP/1.1"; }
        virtual std::string toString() const {
            std::stringstream ss;
            ss << version() << " " << _s << " " << r << kul::os::EOL();
            for(const auto& h : headers()) ss << h.first << ": " << h.second << kul::os::EOL();
            for(const auto& p : cookies()){
                ss << "Set-Cookie: " << p.first << "=" << p.second.value() << "; ";
                if(p.second.domain().size()) ss << "domain=" << p.second.domain() << "; ";
                if(p.second.path().size()) ss << "path=" << p.second.path() << "; ";
                if(p.second.httpOnly()) ss << "httponly; ";
                if(p.second.secure()) ss << "secure; ";
                if(p.second.invalidated()) ss << "expires=Sat, 25-Apr-2015 13:31:44 GMT; maxage=-1; ";
                else
                if(p.second.expires().size()) ss << "expires=" << p.second.expires() << "; ";
                ss << kul::os::EOL();
            }
            ss << kul::os::EOL() << body() << "\r\n" << '\0';
            return ss.str();
        }
        friend std::ostream& operator<<(std::ostream&, const AResponse&);
};
inline std::ostream& operator<<(std::ostream &s, const AResponse& r){
    return s << r.toString();
}

class  _1_1Response : public AResponse{
    public:
        std::string version() const { return "HTTP/1.1"; }
};

class AServer : public kul::tcp::SocketServer<char>{
    protected:
        void asAttributes(std::string a, kul::hash::map::S2S& atts){
            if(a.size() > 0){
                if(a[0] == '?') a = a.substr(1);
                std::vector<std::string> bits;
                kul::String::SPLIT(a, '&', bits);
                for(const std::string& p : bits){
                    if(p.find("=") != std::string::npos){
                        std::vector<std::string> bits = kul::String::SPLIT(p, '=');
                        atts[bits[0]] = bits[1];
                    }else
                        atts[p] = "";
                }
            }
        }
    public:
        AServer(const uint16_t& p) : kul::tcp::SocketServer<char>(p){}
        virtual ~AServer(){}
        
        virtual AResponse respond(const ARequest& req) = 0;
};


}// END NAMESPACE http
}// END NAMESPACE kul

#endif /* _KUL_HTTP_BASE_HPP_ */