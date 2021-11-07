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
#include "mkn/ram/http.hpp"

void mkn::ram::http::A1_1Request::send() KTHROW(mkn::ram::http::Exception) {
  KUL_DBG_FUNC_ENTER
  std::stringstream ss;
  {
    mkn::ram::tcp::Socket<char> sock;
    if (!sock.connect(_host, _port)) KEXCEPTION("TCP FAILED TO CONNECT!");
    const std::string& req(toString());
    sock.write(req.c_str(), req.size());
    std::unique_ptr<char[]> buf(new char[_MKN_RAM_TCP_REQUEST_BUFFER_]);
    int64_t i, d = 0;
    bool more = false;
    do {
      bzero(buf.get(), _MKN_RAM_TCP_REQUEST_BUFFER_);
      more = false;
      d = sock.read(buf.get(), _MKN_RAM_TCP_REQUEST_BUFFER_ - 1, more);
      if (d == -1) return;
      for (i = 0; i < d; i++) ss << buf[i];
    } while (more);
  }
  std::string rec(ss.str());
  _1_1Response res(_1_1Response::FROM_STRING(rec));
  handleResponse(res);
}

class RequestHeaders {
 private:
  mkn::kul::hash::map::S2S _hs;
  RequestHeaders() {
    _hs.insert("Connection", "close");
    _hs.insert("Accept", "text/html");
  }

 public:
  static RequestHeaders& I() {
    static RequestHeaders i;
    return i;
  }
  mkn::kul::hash::map::S2S defaultHeaders(const mkn::ram::http::A1_1Request& r,
                                     const std::string& body = "") const {
    mkn::kul::hash::map::S2S hs1;
    for (const auto& h : _hs)
      if (!r.header("Transfer-Encoding")) hs1.insert(h.first, h.second);
    if (!body.empty() && !r.header("Content-Length") && !r.header("Transfer-Encoding"))
      hs1.insert("Content-Length", std::to_string(body.size()));
    return hs1;
  }
};

std::string mkn::ram::http::_1_1GetRequest::toString() const {
  KUL_DBG_FUNC_ENTER
  std::stringstream ss;
  ss << method() << " /" << _path;
  if (atts.size() > 0) ss << "?";
  for (const std::pair<std::string, std::string>& p : atts) ss << p.first << "=" << p.second << "&";
  if (atts.size() > 0) ss.seekp(-1, ss.cur);
  ss << " " << version();
  ss << "\r\nHost: " << _host;
  for (const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
  for (const auto& h : RequestHeaders::I().defaultHeaders(*this))
    ss << "\r\n" << h.first << ": " << h.second;
  if (cookies().size()) {
    ss << "\r\nCookie: ";
    for (const auto& p : cookies()) ss << p.first << "=" << p.second << "; ";
  }
  ss << "\r\n\r\n";
  return ss.str();
}

std::string mkn::ram::http::_1_1PostRequest::toString() const {
  KUL_DBG_FUNC_ENTER
  std::stringstream ss;
  ss << method() << " /" << _path << " " << version();
  ss << "\r\nHost: " << _host;
  std::stringstream bo;
  if (body().size()) bo << "\r\n" << body();
  for (const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
  for (const auto& h : RequestHeaders::I().defaultHeaders(*this, body()))
    ss << "\r\n" << h.first << ": " << h.second;
  if (cookies().size()) {
    ss << "\r\nCookie: ";
    for (const auto& p : cookies()) ss << p.first << "=" << p.second << "; ";
  }
  ss << "\r\n\r\n";
  if (body().size()) ss << body();
  return ss.str();
}
