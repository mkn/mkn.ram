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
//UPD http://www.binarytides.com/programming-udp-sockets-c-linux/
#include "kul/http.hpp"

#include <arpa/inet.h>

void kul::http::Server::start() throw(kul::http::Exception){
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) KEXCEPTION("HTTP Server error opening socket");
	sockets.push(sockfd);
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = !kul::byte::isBigEndian() ? htons(this->port()) : kul::byte::LittleEndian::UINT32(this->port());
	if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		KEXCEPTION("HTTP Server error on binding");
	::listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	int32_t newsockfd;
	int  r;
	char buffer[256];
	while(1){
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
			KEXCEPTION("HTTP Server error on accept");
		sockets.push(newsockfd);
		bzero(buffer,256);
		r = read(newsockfd, buffer, 255);
		if (r < 0){ KLOG(ERR) << "ERROR reading from socket"; close(newsockfd); continue; }
		std::string b(buffer);
		std::vector<std::string> lines = kul::String::lines(b); 
		std::vector<std::string> l0 = kul::String::split(lines[0], ' ');
		if(!l0.size()){ KLOG(ERR) << "Malformed request found: " << b; close(newsockfd); continue; }
		std::string s(l0[1]);
		std::string a;
		if(l0[0].compare("GET") == 0){
			if(s.find("?") != std::string::npos){
				a = s.substr(s.find("?") + 1);
				s = s.substr(0, s.find("?"));
			}
		}else 
		if(l0[0].compare("POST") == 0){
			bool lb = 0;
			for(uint i = 1; i < lines.size(); i++){
				if(lines[i].size() <= 1 && !lb) lb = 1;
				else if(lb) a += lines[i];
			}
		//}else if(l0[0].compare("HEAD") == 0){
		}else{ 
			KLOG(ERR) << "HTTP Server request type not handled: " << l0[0]; close(newsockfd); continue; 
		}
		const std::pair<kul::hash::set::String, std::string>& p(handle(s, asAttributes(a)));
		std::string ret("HTTP/1.1 200 OK\r\n");
		ret += "Content-Type: text/html\r\n";
		ret += "Server: kserv\r\n";
		ret += "Date: " + kul::DateTime::NOW() + "\r\n";
		ret += "Connection: close\r\n";
		for(const std::string& rh : p.first) ret += rh  + "\r\n";
		for(const std::string& rh : rhs) ret += rh  + "\r\n";
		ret += "\r\n";
		ret += p.second;
		r = ::send(newsockfd, ret.c_str(), ret.length(), 0);
		if (r < 0) KLOG(ERR) << "Error replying to host";
		close(newsockfd);
	}
}
void kul::http::Server::stop(){
	while(!sockets.empty()){
		shutdown(sockets.front(), SHUT_RDWR);
		sockets.pop();
	}
}

class Requester{
	public:
		static std::stringstream send(const std::string& h, const std::string& req, const int& p){
			int32_t sck = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sck < 0) KEXCEPT(kul::http::Exception, "Error opening socket");
			struct sockaddr_in servAddr;
			memset(&servAddr, 0, sizeof(servAddr));
			servAddr.sin_family   = AF_INET;
			int r = 0;
			servAddr.sin_port = !kul::byte::isBigEndian() ? htons(p) : kul::byte::LittleEndian::UINT32(p);
			if(h.compare("localhost") == 0 || h.compare("127.0.0.1") == 0){	
				servAddr.sin_addr.s_addr = INADDR_ANY;
				r = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr));
			}
			else if(inet_pton(AF_INET, &h[0], &(servAddr.sin_addr)))
				r = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr));
			else{
				std::string ip;
				struct addrinfo hints, *servinfo, *next;
				struct sockaddr_in *in;
				memset(&hints, 0, sizeof hints);
    			hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    			hints.ai_socktype = SOCK_STREAM;
    			if ((r = getaddrinfo(h.c_str(), 0, &hints, &servinfo)) != 0) 
    				KEXCEPT(kul::http::Exception, "getaddrinfo failed for host: " + h);
    			for(next = servinfo; next != NULL; next = next->ai_next){
        			in = (struct sockaddr_in *) next->ai_addr;
        			ip = inet_ntoa(in->sin_addr);
        			servAddr.sin_addr.s_addr = inet_addr(&ip[0]);
        			if((r = ::connect(sck, (struct sockaddr*) &servAddr, sizeof(servAddr))) == 0) break;
    			}
    			freeaddrinfo(servinfo);
			}
			if(r < 0) KEXCEPT(kul::http::Exception, "Failed to connect to host: " + h);
			::send(sck, req.c_str(), req.size(), 0); 
			char buffer[10000];
			int d = 0;
			std::stringstream ss;
			while((d = recv(sck, buffer, 10000, 0)) > 0){
				for(int i = 0; i < d; i++) ss << buffer[i];
				memset(&buffer[0], 0, sizeof(buffer));
			}
			::close(sck);
			return ss;
		}
};

void kul::http::_1_1GetRequest::send(const std::string& h, const std::string& res, const int& p){
	try{
		handle(Requester::send(h, toString(h, res), p).str());
	}catch(const kul::Exception& e){
		KEXCEPT(Exception, "HTTP GET fail with host: " + h);
	}
}

void kul::http::_1_1PostRequest::send(const std::string& h, const std::string& res, const int& p){
	try{
		handle(Requester::send(h, toString(h, res), p).str());
	}catch(const kul::Exception& e){
		KEXCEPT(Exception, "HTTP GET fail with host: " + h);
	}
}
