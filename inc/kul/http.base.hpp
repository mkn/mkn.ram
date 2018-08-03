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
#ifndef _KUL_HTTP_BASE_HPP_
#define _KUL_HTTP_BASE_HPP_

#include "kul/map.hpp"
#include "kul/string.hpp"
#include "kul/tcp.hpp"

namespace kul {
namespace http {

typedef std::unordered_map<std::string, std::string> Headers;

class Exception : public kul::Exception {
public:
  Exception(const char *f, const uint16_t &l, const std::string &s)
      : kul::Exception(f, l, s) {}
};

class Cookie {
private:
  bool h = 0, i = 0, s = 0;
  std::string d, p, v, x;

public:
  Cookie(const std::string &v) : v(v) {}
  Cookie &domain(const std::string &d) {
    this->d = d;
    return *this;
  }
  const std::string &domain() const { return d; }
  Cookie &path(const std::string &p) {
    this->p = p;
    return *this;
  }
  const std::string &expires() const { return x; }
  Cookie &expires(const std::string &x) {
    this->x = x;
    return *this;
  }
  const std::string &path() const { return p; }
  const std::string &value() const { return v; }
  Cookie &httpOnly(bool h) {
    this->h = h;
    return *this;
  }
  bool httpOnly() const { return h; }
  Cookie &secure(bool s) {
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
  void header(const std::string &k, const std::string &v) { this->_hs[k] = v; }
  const Headers &headers() const { return _hs; }
  void body(const std::string &b) { this->_b = b; }
  const std::string &body() const { return _b; }
  bool header(const std::string &s) const { return _hs.count(s); }
};

class KUL_PUBLISH _1_1Response : public Message {
protected:
  uint16_t _s = 200;
  std::string r = "OK";
  Headers hs;
  kul::hash::map::S2T<Cookie> cs;

public:
  _1_1Response() {}
  _1_1Response(const std::string &b) { body(b); }
  virtual ~_1_1Response() {}
  void cookie(const std::string &k, const Cookie &c) { cs.insert(k, c); }
  const kul::hash::map::S2T<Cookie> &cookies() const { return cs; }
  const std::string &reason() const { return r; }
  void reason(const std::string &r) { this->r = r; }
  const uint16_t &status() const { return _s; }
  void status(const uint16_t &s) { this->_s = s; }
  virtual std::string version() const { return "HTTP/1.1"; }
  virtual std::string toString() const;
  friend std::ostream &operator<<(std::ostream &, const _1_1Response &);

  _1_1Response &withHeaders(const Headers &heads) {
    for (const auto &p : heads)
      header(p.first, p.second);
    return *this;
  }
  _1_1Response &withCookies(const Cookies &cooks) {
    for (const auto &p : cooks)
      cookie(p.first, p.second);
    return *this;
  }
  _1_1Response &withBody(const std::string &b) {
    body(b);
    return *this;
  }
  virtual _1_1Response &withDefaultHeaders() {
    if (!header("Date"))
      header("Date", kul::DateTime::NOW());
    if (!header("Connection"))
      header("Connection", "close");
    if (!header("Content-Type"))
      header("Content-Type", "text/html");
    if (!header("Content-Length"))
      header("Content-Length", std::to_string(body().size()));
    return *this;
  }

  static _1_1Response FROM_STRING(std::string &);
};
using Response = _1_1Response;
inline std::ostream &operator<<(std::ostream &s, const _1_1Response &r) {
  return s << r.toString();
}

class KUL_PUBLISH A1_1Request : public Message {
protected:
  uint16_t _port;
  std::string _ip, _host, _path;
  kul::hash::map::S2S cs, atts;
  std::function<void(const _1_1Response &)> m_func;

  virtual void handleResponse(const _1_1Response &s) {
    if (m_func)
      m_func(s);
    else
      std::cerr
          << "kul::http::A1_1Request::handleResponse - no response defined"
          << std::endl;
  }

public:
  A1_1Request(const std::string &host, const std::string &path,
              const uint16_t &port, const std::string &ip)
      : _port(port), _ip(ip), _host(host), _path(path) {}
  virtual ~A1_1Request() {}
  virtual std::string method() const = 0;
  virtual std::string toString() const = 0;
  void cookie(const std::string &k, const std::string &v) {
    this->cs.insert(k, v);
  }
  const kul::hash::map::S2S &cookies() const { return cs; }
  A1_1Request &attribute(const std::string &k, const std::string &v) {
    atts[k] = v;
    return *this;
  }
  const kul::hash::map::S2S &attributes() const { return atts; }
  virtual void send() KTHROW(kul::http::Exception);
  const std::string &host() const { return _host; }
  const std::string &path() const { return _path; }
  const std::string &ip() const { return _ip; }
  const uint16_t &port() const { return _port; }
  virtual std::string version() const { return "HTTP/1.1"; }
  A1_1Request &withResponse(const std::function<void(const Response &)> &func) {
    m_func = func;
    return *this;
  }
  A1_1Request &withHeaders(const Headers &heads) {
    for (const auto &p : heads)
      header(p.first, p.second);
    return *this;
  }
  A1_1Request &withCookies(const Headers &cooks) {
    for (const auto &p : cooks)
      cookie(p.first, p.second);
    return *this;
  }
  A1_1Request &withBody(const std::string &b) {
    body(b);
    return *this;
  }
};

class KUL_PUBLISH _1_1GetRequest : public A1_1Request {
public:
  _1_1GetRequest(const std::string &host, const std::string &path = "",
                 const uint16_t &port = 80, const std::string &ip = "")
      : A1_1Request(host, path, port, ip) {}
  virtual ~_1_1GetRequest() {}
  virtual std::string method() const override { return "GET"; }
  virtual std::string toString() const override;
};
using Get = _1_1GetRequest;

class KUL_PUBLISH _1_1PostRequest : public A1_1Request {
public:
  _1_1PostRequest(const std::string &host, const std::string &path = "",
                  const uint16_t &port = 80, const std::string &ip = "")
      : A1_1Request(host, path, port, ip) {}
  virtual std::string method() const override { return "POST"; }
  virtual std::string toString() const override;
};
using Post = _1_1PostRequest;

class KUL_PUBLISH AServer : public kul::tcp::SocketServer<char> {
protected:
  std::function<_1_1Response(const A1_1Request &)> m_func;

  void asAttributes(std::string a, kul::hash::map::S2S &atts) {
    if (a.size() > 0) {
      if (a[0] == '?')
        a = a.substr(1);
      std::vector<std::string> bits;
      kul::String::SPLIT(a, '&', bits);
      for (const std::string &p : bits) {
        if (p.find("=") != std::string::npos) {
          std::vector<std::string> bits = kul::String::SPLIT(p, '=');
          atts[bits[0]] = bits[1];
        } else
          atts[p] = "";
      }
    }
  }

  virtual std::shared_ptr<A1_1Request>
  handleRequest(const int &fd, const std::string &b, std::string &path);

  virtual void handleBuffer(std::map<int, uint8_t> &fds, const int &fd,
                            char *in, const int &read, int &e);

public:
  AServer(const uint16_t &p) : kul::tcp::SocketServer<char>(p) {}
  virtual ~AServer() {}

  AServer &
  withResponse(const std::function<_1_1Response(const A1_1Request &)> &func) {
    m_func = func;
    return *this;
  }

  virtual _1_1Response respond(const A1_1Request &req) {
    if (m_func)
      return m_func(req);
    KEXCEPTION("kul::http::AServer::respond - no response defined");
  };
};

} // END NAMESPACE http
} // END NAMESPACE kul

#endif /* _KUL_HTTP_BASE_HPP_ */