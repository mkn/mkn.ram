/**
Copyright (c) 2024, Philip Deegan.
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
#ifndef _MKN_RAM_TCP_HPP_
#define _MKN_RAM_TCP_HPP_

#include "mkn/kul/dbg.hpp"

namespace mkn {
namespace ram {
namespace tcp {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

template <class T = uint8_t>
class ASocket {
 public:
  virtual ~ASocket() {}
  virtual bool connect(std::string const& host, int16_t const& port) = 0;
  virtual bool close() = 0;
  virtual size_t read(T* data, size_t const& len, bool& more) KTHROW(mkn::ram::tcp::Exception) = 0;
  virtual size_t write(T const* data, size_t const& len) = 0;

 protected:
  bool open = 0;
};

template <class T = uint8_t>
class ASocketServer {
 public:
  virtual ~ASocketServer() {}
  virtual void start() KTHROW(mkn::ram::tcp::Exception) = 0;
  uint64_t up() const { return s - mkn::kul::Now::MILLIS(); }
  uint16_t const& port() const { return p; }
  bool started() const { return s; }

 protected:
  ASocketServer(uint16_t const& p) : p(p) {}

  virtual void onConnect(char const* /*ip*/, uint16_t const& /*port*/) {}
  virtual void onDisconnect(char const* /*ip*/, uint16_t const& /*port*/) {}

 protected:
  uint16_t p;
  uint64_t s;
};

}  // namespace tcp
}  // namespace ram
}  // namespace mkn

#if defined(_WIN32)
#include "mkn/ram/os/win/tcp.hpp"
#else
#include "mkn/ram/os/nixish/tcp.hpp"
#endif

#endif  //_MKN_RAM_TCP_HPP_
