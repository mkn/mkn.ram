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
#ifndef _MKN_RAM_HTTP_SESSION_HPP_
#define _MKN_RAM_HTTP_SESSION_HPP_

#include "mkn/ram/http.hpp"
#include "mkn/ram/http/def.hpp"
#include "mkn/kul/threads.hpp"
#include "mkn/kul/time.hpp"

namespace mkn {
namespace ram {
namespace http {

template <class S>
class SessionServer;

class Session {
 private:
  static const constexpr std::time_t MAX = (std::numeric_limits<std::time_t>::max)();
  std::time_t c;

 public:
  Session() : c(std::time(0)) {}
  void refresh() {
    if (c != MAX) this->c = std::time(0);
  }
  const bool expired() const { return c < (std::time(0) - _MKN_RAM_HTTP_SESSION_TTL_); }
  void invalidate() { c = MAX; }
  template <class S>
  friend class SessionServer;
};

template <class S = Session>
class SessionServer {
 protected:
  mkn::kul::Mutex mutex;
  mkn::kul::Thread th1;
  mkn::kul::hash::map::S2T<std::shared_ptr<S>> sss;
  virtual void operator()() {
    while (true) {
      mkn::kul::this_thread::sleep(_MKN_RAM_HTTP_SESSION_CHECK_);
      {
        auto copy = sss;
        std::vector<std::string> erase;
        for (const auto& p : copy)
          if (p.second->expired()) erase.push_back(p.first);
        mkn::kul::ScopeLock lock(mutex);
        for (const std::string& e : erase) sss.erase(e);
      }
    }
  }

 public:
  SessionServer() : th1(std::ref(*this)) {
    sss.setDeletedKey("DELETED");
    th1.run();
  }
  S* get(const std::string& id) {
    S* s = 0;
    mkn::kul::ScopeLock lock(mutex);
    if (sss.count(id)) s = (*sss.find(id)).second.get();
    if (s && !s->expired()) s->c -= 5;
    return s;
  }
  bool has(const std::string& id) {
    mkn::kul::ScopeLock lock(mutex);
    if (sss.count(id)) {
      S& s = *(*sss.find(id)).second.get();
      if (!s.expired()) s.c -= 5;
    }
    return sss.count(id);
  }
  S& add(const std::string& id, const std::shared_ptr<S>& s) {
    mkn::kul::ScopeLock lock(mutex);
    sss.insert(id, s);
    return *s.get();
  }
  S& add(const std::string& id) { return add(id, std::make_shared<S>()); }
  void refresh(const std::string& id) {
    S* s = 0;
    mkn::kul::ScopeLock lock(mutex);
    if (sss.count(id)) s = (*sss.find(id)).second.get();
    if (s && !s->expired()) (*sss.find(id)).second->refresh();
  }
  void shutdown() {
    mkn::kul::ScopeLock lock(mutex);
    th1.interrupt();
  }
  friend class mkn::kul::Thread;
};
}  // namespace http
}  // namespace ram
}  // namespace mkn
#endif /* _MKN_RAM_HTTP_SESSION_HPP_ */
