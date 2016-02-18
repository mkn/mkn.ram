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
#ifndef _KUL_HTTP_SESSION_HPP_
#define _KUL_HTTP_SESSION_HPP_

#include "kul/http.hpp"
#include "kul/time.hpp"
#include "kul/threads.hpp"
#include "kul/http/def.hpp"

namespace kul{ namespace http{

class Session{
	private:
		uint64_t c;
	public:
		Session() : c(kul::Now::MILLIS()){}
		void refresh(){ 
			this->c = kul::Now::MILLIS(); 
		}
		const bool expired() const { 
			return c < (kul::Now::MILLIS() - (_KUL_HTTP_SESSION_TTL_ * 1000)); 
		}
};

class SessionServer{
	protected:
		kul::Mutex mutex;
		kul::Ref<SessionServer> ref;
		kul::Thread th1;
		kul::hash::map::S2T<std::shared_ptr<Session>> sss;
		virtual void operator()(){
			while(true){
				{
					kul::ScopeLock lock(mutex);
					std::vector<std::string> erase;
					for(const auto& p : sss) if(p.second->expired()) erase.push_back(p.first);					
					for(const std::string& e : erase) sss.erase(e);
				}
				kul::this_thread::sleep(10000);
			}
		}
	public:
		SessionServer() : ref(*this), th1(ref){
			sss.setDeletedKey("DELETED");
			th1.run();
		}
		template <class T = Session>
		T& get(const std::string& id){
			return (*sss.find(id)).second;
		}
		bool has(const std::string& id) const {
			return sss.count(id);
		}
		template <class T = Session>
		T add(const std::string& id, const std::shared_ptr<Session>& s){
			sss.insert(id, s);
			return get(id);
		}
		template <class T = Session>
		T add(const std::string& id){
			sss.insert(id, std::make_shared<T>());
			return get(id);
		}
		void refresh(const std::string& id){
			if(has(id)){
				kul::ScopeLock lock(mutex);
				if(sss.count(id)) (*sss.find(id)).second->refresh();
			}
		}
		void shutdown(){
			kul::ScopeLock lock(mutex);
			th1.interrupt();
		}
		friend class kul::ThreadRef<SessionServer>;
};


}}
#endif /* _KUL_HTTP_SESSION_HPP_ */
