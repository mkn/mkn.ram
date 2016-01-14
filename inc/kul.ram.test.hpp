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
#ifndef _KUL_TEST_HPP_
#define _KUL_TEST_HPP_

#include "kul/http.hpp"
#include "kul/html4.hpp"

#include "kul/signal.hpp"

#ifndef  _KUL_HTTP_TEST_PORT_
#define  _KUL_HTTP_TEST_PORT_ 8888
#endif /*_KUL_HTTP_TEST_PORT_*/

namespace kul{ namespace ram{

class TestHTTPServer : public kul::http::Server{
	private:
		void operator()(){
			start();
		}
	public:
		const kul::http::AResponse response(const std::string& res, const kul::http::ARequest& req){
            kul::http::_1_1Response r;
            r.body("HTTP PROVIDED BY KUL");
            return kul::http::Server::response(r);
        }
		TestHTTPServer() : kul::http::Server(_KUL_HTTP_TEST_PORT_){}
		friend class kul::ThreadRef<TestHTTPServer>;
};

class Get : public kul::http::_1_1GetRequest{
	public:
		void handle(const kul::hash::map::S2S& h, const std::string& b){
			KLOG(INF) << "GET RESPONSE:\n" << b;
		}
};
class Post : public kul::http::_1_1PostRequest{
	public:
		void handle(const kul::hash::map::S2S& h, const std::string& b){
			KLOG(INF) << "POST RESPONSE:\n" << b;
		}
};

class Test{
	public:
		Test(){
			kul::Signal s;
			TestHTTPServer serv;
			auto l = [&serv](int16_t){ serv.stop(); };
			s.intr(l).segv(l);
			kul::Ref<TestHTTPServer> ref(serv);
			kul::Thread t(ref);
			t.run();
			kul::this_thread::sleep(1000);
			if(t.exception()) std::rethrow_exception(t.exception());
			// kul::this_thread::sleep(100000);
			Post().send("localhost", "index.html", _KUL_HTTP_TEST_PORT_);
			if(t.exception()) std::rethrow_exception(t.exception());
			Get() .send("localhost", "index.html", _KUL_HTTP_TEST_PORT_);
			if(t.exception()) std::rethrow_exception(t.exception());
			serv.stop();
			Get() .send("google.com", "", 80);
		}
};

}}
#endif /* _KUL_TEST_HPP_ */