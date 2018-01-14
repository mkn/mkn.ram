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
#include "kul/http.hpp"

std::shared_ptr<kul::http::A1_1Request>
kul::http::AServer::handleRequest(const std::string& b, std::string& path)
{
  KUL_DBG_FUNC_ENTER
  std::string a;
  std::shared_ptr<kul::http::A1_1Request> req;
  {
    std::string mode, host;
    std::stringstream ss(b);
    {
      std::string r;
      std::getline(ss, r);
      std::vector<std::string> l0 = kul::String::SPLIT(r, ' ');
      if (!l0.size())
        KEXCEPTION("Malformed request found: " + b);
      std::string s(l0[1]);
      if (l0[0] == "GET") {
        // req = get();
        if (s.find("?") != std::string::npos) {
          a = s.substr(s.find("?") + 1);
          s = s.substr(0, s.find("?"));
        }
      } else if (l0[0] == "POST") {
      } // req = post();
      else
        KEXCEPTION("HTTP Server request type not handled: " + l0[0]);
      mode = l0[0];
      path = s;
    }

    if (mode == "GET")
      req = std::make_shared<_1_1GetRequest>(host, path, port());
    else if (mode == "POST")
      req = std::make_shared<_1_1PostRequest>(host, path, port());

    {
      std::string l;
      while (std::getline(ss, l)) {
        if (l.size() <= 1)
          break;
        std::vector<std::string> bits;
        kul::String::SPLIT(l, ':', bits);
        kul::String::TRIM(bits[0]);
        std::stringstream sv;
        if (bits.size() > 1)
          sv << bits[1];
        for (size_t i = 2; i < bits.size(); i++)
          sv << ":" << bits[i];
        std::string v(sv.str());
        kul::String::TRIM(v);
        if (*v.rbegin() == '\r')
          v.pop_back();
        if (bits[0] == "Cookie") {
          for (const auto& coo : kul::String::SPLIT(v, ';')) {
            if (coo.find("=") == std::string::npos) {
              req->cookie(coo, "");
              KOUT(ERR) << "Cookie without equals sign, skipping";
            } else {
              std::vector<std::string> kv;
              kul::String::ESC_SPLIT(coo, '=', kv);
              if (kv[1].size())
                req->cookie(kv[0], kv[1]);
            }
          }
        } else
          req->header(bits[0], v);
      }
      std::stringstream ss1;
      while (std::getline(ss, l))
        ss1 << l;
      if (a.empty())
        a = ss1.str();
      req->body(ss1.str());
    }
  }
  kul::hash::map::S2S atts;
  asAttributes(a, atts);
  for (const auto& att : atts)
    req->attribute(att.first, att.second);
  return req;
}

void
kul::http::AServer::handleBuffer(std::map<int, uint8_t>& fds,
                                 const int& fd,
                                 char* in,
                                 const int& read,
                                 int& e)
{
  KUL_DBG_FUNC_ENTER;
  in[read] = '\0';
  std::string res;
  try {
    std::string s(in, read);
    std::string c(s.substr(0, (s.size() > 9) ? 10 : s.size()));
    std::vector<char> allowed = { 'D', 'G', 'P', '/', 'H' };
    bool f = 0;
    for (const auto& ch : allowed) {
      f = c.find(ch) != std::string::npos;
      if (f)
        break;
    }
    if (!f)
      KEXCEPTION(
        "Logic error encountered, probably https attempt on http port");
    std::shared_ptr<A1_1Request> req = handleRequest(s, res);
    const _1_1Response& rs(respond(*req.get()));
    std::string ret(rs.toString());
    e = writeTo(fd, ret.c_str(), ret.length());
  } catch (const kul::http::Exception& e1) {
    KLOG(ERR) << e1.stack();
    e = -1;
  }
  fds[fd] = 1;
}
