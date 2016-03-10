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

class Connection{
    private:
        const std::string i;
        const uint8_t p;
    public:
        Connection(const std::string i, const uint8_t p) : i(i), p(p){}
        const std::string& ip() const { return i; }
        const uint8_t&   port() const { return p; } 
};

class Sendable{
    protected:
        Headers hs;
        std::string b;
    public:
        void header(const std::string& k, const std::string& v){ this->hs.insert(k, v); }
        const Headers& headers() const { return hs; }
        void body(const std::string& b){ this->b = b; }
        const std::string& body() const { return b; }
        bool header(const std::string& s){
            return hs.count(s);
        }
};

class ARequest : public Sendable{
    protected:
        kul::hash::map::S2S cs;
        kul::hash::map::S2S atts;
        virtual void handle(const kul::hash::map::S2S& h, const std::string& s){}
        void handle(std::string b){
            kul::hash::map::S2S h;
            std::string line;
            std::stringstream ss(b);
            while(std::getline(ss, line)){
                if(line.size() <= 2) {
                    b.erase(0, b.find("\n") + 1);
                    break;
                }
                if(line.find(":") != std::string::npos){
                    std::vector<std::string> bits;
                    kul::String::SPLIT(line, ':', bits);
                    std::string l(bits[0]);
                    std::string r(bits[1]);
                    kul::String::TRIM(l);
                    kul::String::TRIM(r);
                    h.insert(l, r);
                }else
                    h.insert(line, "");
                b.erase(0, b.find("\n") + 1);
            }
            handle(h, b);
        }
        const ARequest& request(ARequest& r) const {
            if(!r.header("Connection"))     r.header("Connection", "close");
            if(!r.header("Content-Length")) r.header("Content-Length", std::to_string(r.body().size()));
            if(!r.header("Accept"))         r.header("Accept", "text/html");
            return r; 
        }
    public:
        virtual ~ARequest(){}
        virtual std::string method() const = 0;
        virtual std::string version() const = 0;
        virtual std::string toString(const std::string& host, const std::string& res) = 0;
        void cookie(const std::string& k, const std::string& v) { this->cs.insert(k, v); }
        const kul::hash::map::S2S& cookies() const { return cs; }
        virtual void send(const std::string& host, const std::string& res, const uint16_t& port) = 0;
        ARequest& attribute(const std::string& k, const std::string& v){
            atts[k] = v;
            return *this;
        }
        const kul::hash::map::S2S&  attributes() const { return atts; }
};

class A1_1Request : public ARequest{
    protected:
        std::string version() const { return "HTTP/1.1";}
};

class _1_1GetRequest : public A1_1Request{
    public:
        virtual std::string method() const { return "GET";}
        virtual std::string toString(const std::string& host, const std::string& res){
            std::stringstream ss;
            ss << method() << " /" << res;
            if(atts.size() > 0) ss << "?";
            for(const std::pair<std::string, std::string>& p : atts)
                ss << p.first << "=" << p.second << "&";
            if(atts.size() > 0) ss.seekp(-1, ss.cur);
            ss << " " << version();
            ss << "\r\nHost: " << host;
            request(*this);
            for(const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
            if(cookies().size()){
                ss << "\r\nCookie: ";
                for(const auto& p : cookies()) ss << p.first << "=" << p.second << "; ";
            }
            ss << "\r\n\r\n";
            return ss.str();
        };
        virtual void send(const std::string& host, const std::string& res = "", const uint16_t& port = 80);
};

class _1_1PostRequest : public A1_1Request{
    public:
        virtual std::string method() const { return "POST";}
        virtual std::string toString(const std::string& host, const std::string& res){
            std::stringstream ss;
            ss << method() << " /" << res << " " << version();
            ss << "\r\nHost: " << host;
            std::stringstream bo;
            for(const std::pair<std::string, std::string>& p : atts) bo << p.first << "=" << p.second << "&";
            if(atts.size()){ bo.seekp(-1, ss.cur); bo << " "; }
            if(body().size()) bo << "\r\n" << body();
            body(bo.str());
            request(*this);
            for(const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
            if(cookies().size()){
                ss << "\r\nCookie: ";
                for(const auto& p : cookies()) ss << p.first << "=" << p.second << "; ";
            }
            ss << "\r\n\r\n";
            if(body().size()) ss << body();
            return ss.str();
        }
        virtual void send(const std::string& host, const std::string& res = "", const uint16_t& port = 80);
};

class AResponse : public Sendable{
    protected:
        uint16_t s = 200;
        std::string r = "OK";
        Headers hs;
        kul::hash::map::S2T<Cookie> cs;
    public:
        void cookie(const std::string& k, const Cookie& c){ cs.insert(k, c); }
        const kul::hash::map::S2T<Cookie>& cookies() const { return cs; }
        const std::string& reason() const { return r; }
        void reason(const std::string& r){ this->r = r; }
        const uint16_t& status() const { return s; }
        void status(const uint16_t& s){ this->s = s; }
        virtual std::string version() const { return "HTTP/1.1"; }
        std::string toString()const {
            std::string s;
            toString(s);
            return s;
        }
        virtual void toString(std::string& s) const {
            std::stringstream ss;
            ss << version() << " " << s << " " << r << kul::os::EOL();
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
            s = ss.str();
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

class AServer{
    private:
        std::vector<std::function<void(const Connection&)>> co, di;
    protected:
        uint16_t p;
        uint64_t s;
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
        virtual std::shared_ptr<ARequest> get(){
            return std::make_shared<_1_1GetRequest>();
        }
        virtual std::shared_ptr<ARequest> post(){
            return std::make_shared<_1_1PostRequest>();
        }
        virtual AResponse& response(AResponse& r) const { return r; }
        void onConnect(const std::function<void(const Connection&)>& f){
            co.push_back(f);
        }
        void onConnect(const Connection& c){
            for(const auto& f : co) f(c);
        }
        void onDisconnect(const std::function<void(const Connection&)>& f){
            di.push_back(f);
        }
        void onDisconnect(const Connection& c){
            for(const auto& f : di) f(c);
        }
    public:
        AServer(const uint16_t& p) : p(p), s(kul::Now::MILLIS()){}
        virtual ~AServer(){}
        virtual bool started() const = 0;
        virtual void start() throw(kul::http::Exception) = 0;
        virtual void stop() = 0;
        
        virtual const AResponse response(const std::string& res, const ARequest& req) = 0;
        const uint64_t  up()   const { return s - kul::Now::MILLIS(); }
        const uint16_t& port() const { return p; }
};


}// END NAMESPACE http
}// END NAMESPACE kul

#endif /* _KUL_HTTP_BASE_HPP_ */