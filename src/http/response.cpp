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
#include "kul/http.base.hpp"

std::string
kul::http::_1_1Response::toString() const
{
  std::stringstream ss;
  ss << version() << " " << _s << " " << r << kul::os::EOL();
  for (const auto& h : headers())
    ss << h.first << ": " << h.second << kul::os::EOL();
  for (const auto& p : cookies()) {
    ss << "Set-Cookie: " << p.first << "=" << p.second.value() << "; ";
    if (p.second.domain().size())
      ss << "domain=" << p.second.domain() << "; ";
    if (p.second.path().size())
      ss << "path=" << p.second.path() << "; ";
    if (p.second.httpOnly())
      ss << "httponly; ";
    if (p.second.secure())
      ss << "secure; ";
    if (p.second.invalidated())
      ss << "expires=Sat, 25-Apr-2015 13:33:33 GMT; maxage=-1; ";
    else if (p.second.expires().size())
      ss << "expires=" << p.second.expires() << "; ";
    ss << kul::os::EOL();
  }
  ss << kul::os::EOL() << body() << "\r\n" << '\0';
  return ss.str();
}

kul::http::_1_1Response
kul::http::_1_1Response::FROM_STRING(std::string& b)
{
  _1_1Response res;

  std::stringstream ss(b);
  std::string line;
  std::getline(ss, line);
  auto bits(kul::String::SPLIT(line, " "));
  if (bits.size() > 1)
    res.status(kul::String::UINT16(bits[1]));
  if (bits.size() > 2)
    res.reason(bits[2]);
  b.erase(0, b.find("\n") + 1);
  while (std::getline(ss, line)) {

    if (line.size() <= 2) {
      b.erase(0, b.find("\n") + 1);
      break;
    } else if (line.find("Set-Cookie:") == 0) {
      std::string lef, rig, cook(line.substr(11));
      kul::String::TRIM(cook);
      lef = cook;
      auto eq(cook.find("="));
      if (eq != std::string::npos) {
        lef = cook.substr(0, eq - 1);
        rig = cook.substr(eq);
      }
      res.cookie(lef, rig);
    } else {
      if (line.find(":") != std::string::npos) {
        std::vector<std::string> bits;
        kul::String::SPLIT(line, ':', bits);
        std::string l(bits[0]);
        std::string r(bits[1]);
        kul::String::TRIM(l);
        kul::String::TRIM(r);
        res.header(l, r);
      } else
        res.header(line, "");
    }

    b.erase(0, b.find("\n") + 1);
  }
  // for(uint8_t i = 0; i < 3; i++) b.pop_back();
  res.body(b);
  return res;
}
