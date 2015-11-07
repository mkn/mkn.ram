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
// 
// MAY REQUIRE: runas admin - netsh http add urlacl url=http://localhost:666/ user=EVERYONE listen=yes delegate=no
//
#ifndef _KUL_HTTP_HPP_
#define _KUL_HTTP_HPP_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "kul/log.hpp"
#include "kul/threads.hpp"
#include "kul/http.base.hpp"

#ifndef UNICODE
#define UNICODE
#endif

#ifndef MAX_ULONG_STR
#define MAX_ULONG_STR ((ULONG) sizeof("4294967295"))
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <http.h>
#include <stdio.h>
#include <Windows.h>
#include <winsock2.h>

#pragma comment(lib, "httpapi.lib")
#pragma comment(lib,"ws2_32.lib")

namespace kul{ namespace http{ 

class Server : public kul::http::AServer{
	private:
		const std::string url;
		HANDLE q;

		bool get(PHTTP_REQUEST pRequest);
		bool post(PHTTP_REQUEST pRequest);

		void initialiseReponse(HTTP_RESPONSE& response, int status, PSTR reason);
		void addKnownHeader(HTTP_RESPONSE& response, _HTTP_HEADER_ID headerID, PSTR rawValue);
		void postClean(PUCHAR rstr);
		static const LPVOID wAlloc(ULONG& u){ return HeapAlloc(GetProcessHeap(), 0, u); }
		static const bool wFreeM(LPVOID a){ return HeapFree(GetProcessHeap(), 0, a); }
	public:
		Server(const short& p, const std::string& s = "localhost") : AServer(p), q(NULL), url("http://" + s + ":" + std::to_string(p) + "/"){
			ULONG r = 0;
			r = HttpInitialize(HTTPAPI_VERSION_1, HTTP_INITIALIZE_SERVER, NULL);
			if(r != NO_ERROR) KEXCEPT(Exception, "HttpInitialize failed: " + std::to_string(r));
			r = HttpCreateHttpHandle(&this->q, 0);
			if(r != NO_ERROR) KEXCEPT(Exception, "HttpCreateHttpHandle failed: " + std::to_string(r));
			std::wstring ws(url.begin(), url.end());
			r = HttpAddUrl(this->q, ws.c_str(), NULL);
			if(r != NO_ERROR) KEXCEPT(Exception, "HttpAddUrl failed: " + std::to_string(r));
		}
		~Server(){
			HttpRemoveUrl(this->q, std::wstring(url.begin(), url.end()).c_str());
			if(q) CloseHandle(q);
			HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
		}
		void start() throw(kul::http::Exception);
		bool started(){ return q != NULL; }
		void stop();

		virtual const std::pair<kul::hash::set::String, std::string> handle(const std::string& res, kul::hash::map::S2S atts){
			using namespace std;
			stringstream ss;
			ss << res << " : ";	
			for(const pair<string, string> p : atts) ss << p.first << "=" << p.second << " ";
			return pair<kul::hash::set::String, string>(
				kul::hash::set::String(), res + " : " + ss.str());
		}
};

}// END NAMESPACE ipc
}// END NAMESPACE http

#endif /* _KUL_HTTP_HPP_ */
