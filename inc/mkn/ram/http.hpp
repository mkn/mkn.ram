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
//
// MAY REQUIRE: runas admin - netsh http add urlacl url=http://localhost:666/
// user=EVERYONE listen=yes delegate=no
//
#ifndef _MKN_RAM_HTTP_HPP_
#define _MKN_RAM_HTTP_HPP_

#include "mkn/kul/map.hpp"
#include "mkn/kul/string.hpp"
#include "mkn/ram/tcp.hpp"

namespace mkn {
namespace ram {
namespace http {

typedef std::unordered_map<std::string, std::string> Headers;

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

class Cookie {
 private:
  bool h = 0, i = 0, s = 0;
  std::string d, p, v, x;

 public:
  Cookie(std::string const& v) : v(v) {}
  Cookie& domain(std::string const& d) {
    this->d = d;
    return *this;
  }
  std::string const& domain() const { return d; }
  Cookie& path(std::string const& p) {
    this->p = p;
    return *this;
  }
  std::string const& expires() const { return x; }
  Cookie& expires(std::string const& x) {
    this->x = x;
    return *this;
  }
  std::string const& path() const { return p; }
  std::string const& value() const { return v; }
  Cookie& httpOnly(bool h) {
    this->h = h;
    return *this;
  }
  bool httpOnly() const { return h; }
  Cookie& secure(bool s) {
    this->s = s;
    return *this;
  }
  bool secure() const { return s; }
  bool invalidated() const { return i; }
  void invalidate() {
    v = "";
    x = "";
    i = 1;
  }
};
typedef std::unordered_map<std::string, Cookie> Cookies;

class Message {
 protected:
  Headers _hs;
  std::string _b = "";

 public:
  void header(std::string const& k, std::string const& v) { this->_hs[k] = v; }
  Headers const& headers() const { return _hs; }
  void body(std::string const& b) { this->_b = b; }
  std::string const& body() const { return _b; }
  bool header(std::string const& s) const { return _hs.count(s); }
};

class KUL_PUBLISH _1_1Response : public Message {
 protected:
  uint16_t _s = 200;
  std::string r = "OK";
  Headers hs;
  mkn::kul::hash::map::S2T<Cookie> cs;

 public:
  _1_1Response() {}
  _1_1Response(std::string const& b) { body(b); }
  virtual ~_1_1Response() {}
  void cookie(std::string const& k, Cookie const& c) { cs.insert(k, c); }
  mkn::kul::hash::map::S2T<Cookie> const& cookies() const { return cs; }
  std::string const& reason() const { return r; }
  void reason(std::string const& r) { this->r = r; }
  uint16_t const& status() const { return _s; }
  void status(uint16_t const& s) { this->_s = s; }
  virtual std::string version() const { return "HTTP/1.1"; }
  virtual std::string toString() const;
  friend std::ostream& operator<<(std::ostream&, _1_1Response const&);

  _1_1Response& withHeaders(Headers const& heads) {
    for (auto const& p : heads) header(p.first, p.second);
    return *this;
  }
  _1_1Response& withCookies(Cookies const& cooks) {
    for (auto const& p : cooks) cookie(p.first, p.second);
    return *this;
  }
  _1_1Response& withBody(std::string const& b) {
    body(b);
    return *this;
  }
  virtual _1_1Response& withDefaultHeaders() {
    if (!header("Date")) header("Date", mkn::kul::DateTime::NOW());
    if (!header("Connection")) header("Connection", "close");
    if (!header("Content-Type")) header("Content-Type", "text/html");
    if (!header("Content-Length")) header("Content-Length", std::to_string(body().size()));
    return *this;
  }

  static _1_1Response FROM_STRING(std::string&);
};
using Response = _1_1Response;
inline std::ostream& operator<<(std::ostream& s, _1_1Response const& r) {
  return s << r.toString();
}

class KUL_PUBLISH A1_1Request : public Message {
 protected:
  uint16_t _port;
  std::string _ip, _host, _path;
  mkn::kul::hash::map::S2S cs, atts;
  std::function<void(_1_1Response const&)> m_func;

  virtual void handleResponse(_1_1Response const& s) {
    if (m_func)
      m_func(s);
    else
      std::cerr << "mkn::ram::http::A1_1Request::handleResponse - no response defined" << std::endl;
  }

 public:
  A1_1Request(std::string const& host, std::string const& path, uint16_t const& port,
              std::string const& ip)
      : _port(port), _ip(ip), _host(host), _path(path) {}
  virtual ~A1_1Request() {}
  virtual std::string method() const = 0;
  virtual std::string toString() const = 0;
  void cookie(std::string const& k, std::string const& v) { this->cs.insert(k, v); }
  mkn::kul::hash::map::S2S const& cookies() const { return cs; }
  A1_1Request& attribute(std::string const& k, std::string const& v) {
    atts[k] = v;
    return *this;
  }
  mkn::kul::hash::map::S2S const& attributes() const { return atts; }
  virtual void send() KTHROW(mkn::ram::http::Exception);
  std::string const& host() const { return _host; }
  std::string const& path() const { return _path; }
  std::string const& ip() const { return _ip; }
  uint16_t const& port() const { return _port; }
  virtual std::string version() const { return "HTTP/1.1"; }
  A1_1Request& withResponse(std::function<void(Response const&)> const& func) {
    m_func = func;
    return *this;
  }
  A1_1Request& withHeaders(Headers const& heads) {
    for (auto const& p : heads) header(p.first, p.second);
    return *this;
  }
  A1_1Request& withCookies(Headers const& cooks) {
    for (auto const& p : cooks) cookie(p.first, p.second);
    return *this;
  }
  A1_1Request& withBody(std::string const& b) {
    body(b);
    return *this;
  }
};

class KUL_PUBLISH _1_1GetRequest : public A1_1Request {
 public:
  _1_1GetRequest(std::string const& host, std::string const& path = "", uint16_t const& port = 80,
                 std::string const& ip = "")
      : A1_1Request(host, path, port, ip) {}
  virtual ~_1_1GetRequest() {}
  virtual std::string method() const override { return "GET"; }
  virtual std::string toString() const override;
};
using Get = _1_1GetRequest;

class KUL_PUBLISH _1_1PostRequest : public A1_1Request {
 public:
  _1_1PostRequest(std::string const& host, std::string const& path = "", uint16_t const& port = 80,
                  std::string const& ip = "")
      : A1_1Request(host, path, port, ip) {}
  virtual std::string method() const override { return "POST"; }
  virtual std::string toString() const override;
};
using Post = _1_1PostRequest;

class KUL_PUBLISH AServer : public mkn::ram::tcp::SocketServer<char> {
 protected:
  std::function<_1_1Response(A1_1Request const&)> m_func;

  void asAttributes(std::string a, mkn::kul::hash::map::S2S& atts) {
    if (a.size() > 0) {
      if (a[0] == '?') a = a.substr(1);
      std::vector<std::string> bits;
      mkn::kul::String::SPLIT(a, '&', bits);
      for (std::string const& p : bits) {
        if (p.find("=") != std::string::npos) {
          std::vector<std::string> bits = mkn::kul::String::SPLIT(p, '=');
          atts[bits[0]] = bits[1];
        } else
          atts[p] = "";
      }
    }
  }

  virtual std::shared_ptr<A1_1Request> handleRequest(int const& fd, std::string const& b,
                                                     std::string& path);

  virtual void handleBuffer(std::map<int, uint8_t>& fds, int const& fd, char* in, int const& read,
                            int& e);

 public:
  AServer(uint16_t const& p) : mkn::ram::tcp::SocketServer<char>(p) {}
  virtual ~AServer() {}

  AServer& withResponse(std::function<_1_1Response(A1_1Request const&)> const& func) {
    m_func = func;
    return *this;
  }

  virtual _1_1Response respond(A1_1Request const& req) {
    if (m_func) return m_func(req);
    KEXCEPTION("mkn::ram::http::AServer::respond - no response defined");
  };
};

}  // namespace http
}  // namespace ram
}  // namespace mkn

#if defined(_WIN32)
#include "mkn/ram/os/win/http.hpp"
#else
#include "mkn/ram/os/nixish/http.hpp"
#endif

#endif /* _MKN_RAM_HTTP_HPP_ */
