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
#include <cstring>

#include "kul/tcp.hpp"
#include "kul/http.hpp"

#ifdef  _KUL_HTTPS_
#include "kul/https.hpp"
#endif//_KUL_HTTPS_

#include "kul/html4.hpp"

#include "kul/signal.hpp"

#ifndef  _KUL_HTTP_TEST_PORT_
#define  _KUL_HTTP_TEST_PORT_ 8888
#endif /*_KUL_HTTP_TEST_PORT_*/

namespace kul{ namespace ram{

void addResponseHeaders(kul::http::AResponse& r){
    if(!r.header("Date"))           r.header("Date", kul::DateTime::NOW());
    if(!r.header("Connection"))     r.header("Connection", "close");
    if(!r.header("Content-Type"))   r.header("Content-Type", "text/html");
    if(!r.header("Content-Length")) r.header("Content-Length", std::to_string(r.body().size()));
}

class TestHTTPServer : public kul::http::Server{
    private:
        void operator()(){
            start();
        }
    public:
        kul::http::AResponse respond(const kul::http::ARequest& req){
            KUL_DBG_FUNC_ENTER
            kul::http::_1_1Response r;
            r.body("HTTP PROVIDED BY KUL");
            addResponseHeaders(r);
            return r;
        }
        TestHTTPServer() : kul::http::Server(_KUL_HTTP_TEST_PORT_){}
        friend class kul::Thread;
};

class TestMultiHTTPServer : public kul::http::MultiServer{
    private:
        void operator()(){
            start();
        }
    public:
        kul::http::AResponse respond(const kul::http::ARequest& req){
            KUL_DBG_FUNC_ENTER
            kul::http::_1_1Response r;
            r.body("MULTI HTTP PROVIDED BY KUL");
            addResponseHeaders(r);
            return r;
        }
        TestMultiHTTPServer() : kul::http::MultiServer(_KUL_HTTP_TEST_PORT_, 3){}
        friend class kul::Thread;
};

#ifdef  _KUL_HTTPS_
class TestHTTPSServer : public kul::https::Server{
    private:
        void operator()(){
            start();
        }
    public:
        kul::http::AResponse respond(const kul::http::ARequest& req){
            KUL_DBG_FUNC_ENTER
            kul::http::_1_1Response r;
            r.body("HTTPS PROVIDED BY KUL: " + req.method());
            addResponseHeaders(r);
            return r;
        }
        TestHTTPSServer() : kul::https::Server(
            _KUL_HTTP_TEST_PORT_,
            kul::File("res/test/server.crt"),
            kul::File("res/test/server.key")
        ){}
        friend class kul::Thread;
};

class TestMultiHTTPSServer : public kul::https::MultiServer{
    private:
        void operator()(){
            start();
        }
    public:
        kul::http::AResponse respond(const kul::http::ARequest& req){
            KUL_DBG_FUNC_ENTER
            kul::http::_1_1Response r;
            r.body("MULTI HTTPS PROVIDED BY KUL");
            addResponseHeaders(r);
            return r;
        }
        TestMultiHTTPSServer() : kul::https::MultiServer(
            _KUL_HTTP_TEST_PORT_, 3,
            kul::File("res/test/server.crt"),
            kul::File("res/test/server.key")){
        }
        friend class kul::Thread;
};


class HTTPS_Get : public kul::https::_1_1GetRequest{
    public:
        HTTPS_Get(const std::string& host, const std::string& path = "", const uint16_t& port = 80) 
            : kul::https::_1_1GetRequest(host, path, port){
        }
        void handleResponse(const kul::hash::map::S2S& h, const std::string& b) override {
            KLOG(INF) << "HTTPS GET RESPONSE:\n" << b;
        }
};
class HTTPS_Post : public kul::https::_1_1PostRequest{
    public:
        HTTPS_Post(const std::string& host, const std::string& path = "", const uint16_t& port = 80) 
            : kul::https::_1_1PostRequest(host, path, port){
        }
        void handleResponse(const kul::hash::map::S2S& h, const std::string& b) override {
            KUL_DBG_FUNC_ENTER
            for(const auto& p : h)
                KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
            KOUT(NON) << "HTTPS POST RESPONSE:\n" << b;
        }
};
#endif//_KUL_HTTPS_

class TestSocketServer : public kul::tcp::SocketServer<char>{
    private:
        void operator()(){
            start();
        }
    public:
        bool handle(char* in, char* out) override {
            KUL_DBG_FUNC_ENTER
            std::strcpy(out, "TCP PROVIDED BY KUL");
            return true; // if true, close connection
        }
        TestSocketServer() : kul::tcp::SocketServer<char>(_KUL_HTTP_TEST_PORT_){}
        friend class kul::Thread;
};

class Get : public kul::http::_1_1GetRequest{
    public:
        Get(const std::string& host, const std::string& path = "", const uint16_t& port = 80) 
            : kul::http::_1_1GetRequest(host, path, port){
        }
        void handleResponse(const kul::hash::map::S2S& h, const std::string& b) override {
            KLOG(INF) << "GET RESPONSE:\n" << b;
        }
};
class Post : public kul::http::_1_1PostRequest{
    public:
        Post(const std::string& host, const std::string& path = "", const uint16_t& port = 80) 
            : kul::http::_1_1PostRequest(host, path, port){
        }
        void handleResponse(const kul::hash::map::S2S& h, const std::string& b) override {
            KUL_DBG_FUNC_ENTER
            for(const auto& p : h)
                KOUT(NON) << "HEADER: " << p.first << " : " << p.second;
            KOUT(NON) << "POST RESPONSE:\n" << b;
        }
};

class Test{
    public:
        Test(){
#ifdef  _KUL_HTTPS_
            KOUT(NON) << "Single HTTPS SERVER";
            {
                TestHTTPSServer serv;
                serv.init();
                kul::Thread t(std::ref(serv));
                t.run();
                kul::this_thread::sleep(100);
                if(t.exception()) std::rethrow_exception(t.exception());
                {
                    HTTPS_Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();
                    if(t.exception()) std::rethrow_exception(t.exception());
                    HTTPS_Post p("localhost", "index.html", _KUL_HTTP_TEST_PORT_);
                    p.body("tsop");
                    p.send();
                    if(t.exception()) std::rethrow_exception(t.exception());
                    HTTPS_Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();
                    if(t.exception()) std::rethrow_exception(t.exception());
                }
                kul::this_thread::sleep(100);
                serv.stop();
                t.join();
                kul::this_thread::sleep(100);
            }
            KOUT(NON) << "Multi HTTPS SERVER";
            {
                TestMultiHTTPSServer serv;
                serv.init();
                kul::Thread t(std::ref(serv));
                t.run();
                for(size_t i = 0; i < 10; i++){
                    if(t.exception()) std::rethrow_exception(t.exception());
                    HTTPS_Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();                    
                }
                kul::this_thread::sleep(100);
                serv.stop();
                kul::this_thread::sleep(100);
                serv.join();
                t.join();
            }
#endif//_KUL_HTTPS_
            KOUT(NON) << "Single HTTP SERVER";
            {
                TestHTTPServer serv;
                kul::Thread t(std::ref(serv));
                t.run();
                kul::this_thread::sleep(100);
                if(t.exception()) std::rethrow_exception(t.exception());
                {
                    Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();
                    KLOG(INF);
                    Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();
                    KLOG(INF);
                    if(t.exception()) std::rethrow_exception(t.exception());
                    KLOG(INF);
                    Post p("localhost", "index.html", _KUL_HTTP_TEST_PORT_);
                    KLOG(INF);
                    p.body("tsop");
                    KLOG(INF);
                    p.send();
                    KLOG(INF);
                    if(t.exception()) std::rethrow_exception(t.exception());
                    KLOG(INF);
                }
                kul::this_thread::sleep(100);
                serv.stop();
                KLOG(INF);
                kul::this_thread::sleep(100);
                KLOG(INF);
                t.join();
            }
            KOUT(NON) << "TCP Socket SERVER";
            {
                kul::tcp::Socket<char> sock; 
                if(!sock.connect("google.com", 80)) KEXCEPTION("TCP FAILED TO CONNECT!");
                Get get("google.com");
                std::string req(get.toString());
                std::vector<char> v1, v2;
                KOUT(NON) << "Writing to TCP socket";
                for(size_t i = 0; i < req.size() / 2; i++) v1.push_back(req.at(i));
                for(size_t i = req.size() / 2; i < req.size(); i++) v2.push_back(req.at(i));
                std::string s1(v1.begin(), v1.end()), s2(v2.begin(), v2.end());
                sock.write(s1.c_str(), s1.size());
                sock.write(s2.c_str(), s2.size());
                KOUT(NON) << "Reading from TCP socket";
                char buf[_KUL_TCP_REQUEST_BUFFER_];
                bzero(buf, _KUL_TCP_REQUEST_BUFFER_);
                sock.read(buf, _KUL_TCP_REQUEST_BUFFER_);
                KLOG(INF) << buf;

                sock.close();
            }
            KLOG(INF);
            kul::this_thread::sleep(500);
            {
                TestSocketServer serv;
                kul::Thread t(std::ref(serv));
                t.run();
                kul::this_thread::sleep(100);
                if(t.exception()) std::rethrow_exception(t.exception());

                kul::tcp::Socket<char> sock; 
                if(!sock.connect("localhost", _KUL_HTTP_TEST_PORT_)) KEXCEPTION("TCP FAILED TO CONNECT!");

                const char* c = "socketserver";
                sock.write(c, strlen(c));

                char buf[_KUL_TCP_REQUEST_BUFFER_];
                bzero(buf, _KUL_TCP_REQUEST_BUFFER_);
                sock.read(buf, _KUL_TCP_REQUEST_BUFFER_);
                KLOG(INF) << buf;

                sock.close();
                serv.stop();
                t.join();
            }
            KLOG(INF);
            kul::this_thread::sleep(100);
            {
                TestMultiHTTPServer serv;
                kul::Thread t(std::ref(serv));
                t.run();
                for(size_t i = 0; i < 10; i++){
                    Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();   
                    if(t.exception()) std::rethrow_exception(t.exception());
                }
                kul::this_thread::sleep(100);
                serv.stop();
                kul::this_thread::sleep(100);
                serv.join();
            }
            KOUT(NON) << "FINISHED!";
        }
};

}}

#ifndef __KUL_RAM_NOMAIN__
int main(int argc, char* argv[]){
    kul::Signal s;
    try{
        kul::ram::Test();
    }catch(const kul::Exception& e){ 
        KERR << e.stack(); return 1;
    }catch(const std::exception& e){ 
        KERR << e.what();  return 2;
    }catch(...){ 
        KERR << "UNKNOWN EXCEPTION CAUGHT";  return 3;
    }
    return 0;
}
#endif//__KUL_RAM_NOMAIN__
