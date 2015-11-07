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
#ifndef _KUL_HTTP_HPP_
#define _KUL_HTTP_HPP_

#include "kul/log.hpp"
#include "kul/byte.hpp"
#include "kul/time.hpp"
#include "kul/http.base.hpp"

#include <netdb.h>
#include <stdio.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

namespace kul{ namespace http{

class Server : public kul::http::AServer{
	private:
		int32_t sockfd;
		socklen_t clilen;
		struct sockaddr_in serv_addr, cli_addr;

		std::queue<int> sockets;
	public:
		Server(const short& p, const std::string& w = "localhost") : AServer(p){}
		void start() throw(kul::http::Exception);
		bool started(){ return sockets.size(); }
		void stop();
		virtual const std::pair<kul::hash::set::String, std::string> handle(const std::string& res, kul::hash::map::S2S atts){
			using namespace std;
			kul::hash::set::String set;
			stringstream ss;
			ss << res << " : ";	
			for(const pair<string, string> p : atts)
				ss << p.first << "=" << p.second << " ";
			set.insert("Content-Length: " + std::to_string(ss.str().size()));
			return std::pair<kul::hash::set::String, std::string>(set, ss.str());
		}
};


}}
#endif /* _KUL_HTTP_HPP_ */
