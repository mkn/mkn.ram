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

void kul::http::ARequest::handle(std::string b){
    KLOG(DBG);
    kul::hash::map::S2S h;
    std::string line;
    std::stringstream ss(b);
    while(std::getline(ss, line)){
        if(line.size() <= 2) {
            b.erase(0, b.find("\n") + 1);
            break;
        }
        if(line.find(":") != std::string::npos){
            std::vector<std::string> bits;
            kul::String::SPLIT(line, ':', bits);
            std::string l(bits[0]);
            std::string r(bits[1]);
            kul::String::TRIM(l);
            kul::String::TRIM(r);
            h.insert(l, r);
        }else
            h.insert(line, "");
        b.erase(0, b.find("\n") + 1);
    }
    handleResponse(h, b);
    KLOG(DBG);
}

std::string kul::http::_1_1GetRequest::toString() const {
    KLOG(DBG);
    std::stringstream ss;
    ss << method() << " /" << _path;
    if(atts.size() > 0) ss << "?";
    for(const std::pair<std::string, std::string>& p : atts)
        ss << p.first << "=" << p.second << "&";
    if(atts.size() > 0) ss.seekp(-1, ss.cur);
    ss << " " << version();
    ss << "\r\nHost: " << _host;
    for(const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
    if(cookies().size()){
        ss << "\r\nCookie: ";
        for(const auto& p : cookies()) ss << p.first << "=" << p.second << "; ";
    }
    ss << "\r\n\r\n";
    KLOG(DBG);
    return ss.str();
}

std::string kul::http::_1_1PostRequest::toString() const {
    KLOG(DBG);
    std::stringstream ss;
    ss << method() << " /" << _path << " " << version();
    ss << "\r\nHost: " << _host;
    std::stringstream bo;
    for(const std::pair<std::string, std::string>& p : atts) bo << p.first << "=" << p.second << "&";
    if(atts.size()){ bo.seekp(-1, ss.cur); bo << " "; }
    if(body().size()) bo << "\r\n" << body();
    std::string bod(bo.str());
    for(const auto& h : headers()) ss << "\r\n" << h.first << ": " << h.second;
    if(cookies().size()){
        ss << "\r\nCookie: ";
        for(const auto& p : cookies()) ss << p.first << "=" << p.second << "; ";
    }
    ss << "\r\n";
    if(bod.size()) ss << bod;
    KLOG(DBG);
    return ss.str();
}
