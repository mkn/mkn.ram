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
typedef kul::hash::set::String Cookies;

class Exception : public kul::Exception{
    public:
        Exception(const char*f, const uint16_t& l, const std::string& s) : kul::Exception(f, l, s){}
};

class Sendable{
    protected:
        Headers hs;
        Cookies cs;
        std::string b;
    public:
        void cookie(const std::string& k){ this->cs.insert(k); }
        const Cookies& cookies() const { return cs; }
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
        kul::hash::map::S2S atts;
        kul::hash::set::String mis;
        virtual const std::string method() const = 0;
        virtual const std::string version() const = 0;
        virtual const std::string toString(const std::string& host, const std::string& res) = 0;
        virtual void handle(const kul::hash::map::S2S& h, const std::string& s){
            printf("%s", s.c_str());
        }
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
                    kul::String::split(line, ':', bits);
                    std::string l(bits[0]);
                    std::string r(bits[1]);
                    kul::String::trim(l);
                    kul::String::trim(r);
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
            return r; 
        }
    public:
        ARequest(bool text = 1) { if(text) mime("text/html");}
        virtual ~ARequest(){}
        virtual void send(const std::string& host, const std::string& res, const uint16_t& port) = 0;
        ARequest& attribute(const std::string& k, const std::string& v){
            atts[k] = v;
            return *this;
        }
        ARequest& mime(const std::string& m){
            mis.insert(m);
            return *this;
        }
        const kul::hash::map::S2S&  attributes() const { return atts; }
};

class A1_1Request : public ARequest{
    protected:
        const std::string version() const{ return "HTTP/1.1";}
};

class _1_1GetRequest : public A1_1Request{
    private:
        const std::string method() const { return "GET";}
    protected:
        virtual const std::string toString(const std::string& host, const std::string& res){
            std::stringstream ss;
            ss << method() << " /" << res;
            if(atts.size() > 0) ss << "?";
            for(const std::pair<std::string, std::string>& p : atts)
                ss << p.first << "=" << p.second << "&";
            if(atts.size() > 0) ss.seekp(-1, ss.cur);
            ss << " " << version();
            ss << "\r\nHost: " << host;
            if(mis.size() > 0 ) ss << "\r\nAccept: ";
            for(const std::string& m : mis) ss << m + ", ";
            if(mis.size() > 0 ) ss.seekp(-2, ss.cur);
            request(*this);
            for(const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
            for(const auto& c : cookies()) ss << "\r\nCookie: " << c;
            ss << "\r\n\r\n";
            return ss.str();
        };
    public:
        virtual void send(const std::string& host, const std::string& res = "", const uint16_t& port = 80);
};

class _1_1PostRequest : public A1_1Request{
    private:
        const std::string method() const { return "POST";}
    protected:
        virtual const std::string toString(const std::string& host, const std::string& res){
            std::stringstream ss;
            ss << method() << " /" << res << " " << version();
            ss << "\r\nHost: " << host;
            if(mis.size() > 0 ) ss << "\r\nAccept: ";
            for(const std::string& m : mis) ss << m << ", ";
            if(mis.size() > 0 ) ss.seekp(-2, ss.cur);
            std::stringstream bo;
            for(const std::pair<std::string, std::string>& p : atts) bo << p.first << "=" << p.second << "&";
            if(atts.size() > 0){ bo.seekp(-1, ss.cur); bo << " "; }
            body(bo.str());
            request(*this);
            for(const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
            for(const auto& c : cookies()) ss << "\r\nCookie: " << c;
            ss << "\r\n\r\n";
            ss << body();
            return ss.str();
        }
    public:
        virtual void send(const std::string& host, const std::string& res = "", const uint16_t& port = 80);
};

class AResponse : public Sendable{
    protected:
        uint16_t s = 200;
        std::string r = "OK";
        Headers hs;
    public:
        const std::string& reason() const { return r; }
        void reason(const std::string& r){ this->r = r; }
        const uint16_t& status() const { return s; }
        void status(const uint16_t& s){ this->s = s; }
        virtual const std::string version() const { return "HTTP/1.1"; }
};

class  _1_1Response : public AResponse{
    public:
        const std::string version() const { return "HTTP/1.1"; }
};

class AServer{
    protected:
        uint16_t p;
        uint64_t s;
        void asAttributes(std::string a, kul::hash::map::S2S& atts){
            if(a.size() > 0){
                if(a[0] == '?') a = a.substr(1);
                std::vector<std::string> bits;
                kul::String::split(a, '&', bits);
                for(const std::string& p : bits){
                    if(p.find("=") != std::string::npos){
                        std::vector<std::string> bits = kul::String::split(p, '=');
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
        virtual AResponse& response(AResponse& r){ return r; }
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