/**
Copyright (c) 2026, Philip Deegan.
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
// HTTP/2 client (RFC 7540) - TLS+ALPN "h2" only, client-initiated streams only.
// See mkn/ram/os/nixish/http2.hpp for the Client that actually opens connections
// and mkn/ram/http2/{frame,hpack}.hpp for the wire-format/compression layers.
#ifndef _MKN_RAM_HTTP2_HPP_
#define _MKN_RAM_HTTP2_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "mkn/kul/except.hpp"
#include "mkn/ram/http2/def.hpp"
#include "mkn/ram/http2/frame.hpp"
#include "mkn/ram/http2/hpack.hpp"

namespace mkn {
namespace ram {
namespace http2 {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

typedef std::unordered_map<std::string, std::string> Headers;

class Request {
 protected:
  std::string _host, _path, _method = "GET", _scheme = "https", _body;
  uint16_t _port;
  Headers _hs;

 public:
  Request(std::string const& host, std::string const& path = "/", uint16_t const& port = 443)
      : _host(host), _path(path.empty() ? "/" : path), _port(port) {}

  std::string const& host() const { return _host; }
  std::string const& path() const { return _path; }
  std::string const& method() const { return _method; }
  std::string const& scheme() const { return _scheme; }
  std::string const& body() const { return _body; }
  uint16_t const& port() const { return _port; }
  Headers const& headers() const { return _hs; }

  Request& withMethod(std::string const& m) {
    _method = m;
    return *this;
  }
  Request& withScheme(std::string const& s) {
    _scheme = s;
    return *this;
  }
  Request& withBody(std::string const& b) {
    _body = b;
    return *this;
  }
  Request& withHeader(std::string const& k, std::string const& v) {
    _hs[k] = v;
    return *this;
  }
  Request& withHeaders(Headers const& hs) {
    for (auto const& p : hs) _hs[p.first] = p.second;
    return *this;
  }
};

// Response headers are kept as an ordered list (not a map) because HTTP/2 allows
// duplicate header names (e.g. repeated "set-cookie") and pseudo-headers must stay
// first, matching what hpack::Decoder produces.
class Response {
 protected:
  uint16_t _status = 0;
  hpack::Headers _hs;
  std::string _b;

 public:
  void status(uint16_t s) { _status = s; }
  uint16_t const& status() const { return _status; }
  void header(std::string const& k, std::string const& v) { _hs.emplace_back(k, v); }
  hpack::Headers const& headers() const { return _hs; }
  std::string header(std::string const& k) const {
    for (auto const& p : _hs)
      if (p.first == k) return p.second;
    return "";
  }
  bool hasHeader(std::string const& k) const {
    for (auto const& p : _hs)
      if (p.first == k) return true;
    return false;
  }
  void body(std::string const& b) { _b = b; }
  void appendBody(char const* d, size_t n) { _b.append(d, n); }
  std::string const& body() const { return _b; }
};

}  // namespace http2
}  // namespace ram
}  // namespace mkn

#if !defined(_WIN32)
#include "mkn/ram/os/nixish/http2.hpp"
#endif /* !defined(_WIN32) - HTTP/2 client is POSIX-only for now */

#endif /* _MKN_RAM_HTTP2_HPP_ */
