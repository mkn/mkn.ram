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
#ifndef _KUL_GIT_GIST_HPP_
#define _KUL_GIT_GIST_HPP_

//
// REQUIRES parse.json
//

#include "kul/io.hpp"
#include "kul/http.hpp"

#include <json/reader.h>
#include <json/writer.h>

namespace kul{ namespace git{ namespace gist{

class Exception : public kul::Exception{
    public:
        Exception(const char*f, const uint16_t& l, const std::string& s) : kul::Exception(f, l, s){}
};
}// END NAMESPACE gist

class Post : public kul::http::_1_1PostRequest{
    private:
        Json::Value j;
        bool f = 0;
    public:
        void handle(const kul::hash::map::S2S& h, const std::string& b){
            Json::Reader reader;
            f = !reader.parse(b.c_str(), j);
            if(!f) KLOG(DBG) << j;
        }
        bool fail(){ return f; }
        const Json::Value json(){ return j; }
};
class Gist{

    public:
        static std::string CREATE(const std::string& desc, const std::vector<kul::File>& files, bool pub = 0) throw(Exception) {
            Json::Value json;
            json["description"] = desc;
            if(pub) json["public"] = true;
            for(const auto& f : files){
                std::stringstream ss;
                const std::string* s = 0;
                kul::io::Reader r(f);
                while((s = r.readLine())) ss << *s;
                Json::Value content;
                content["content"] = ss.str();
                Json::Value file;
                file[f.name()].append(content);
                json["files"].append(file);
            }
            std::stringstream ss;
            ss << Json::FastWriter().write(json);
            Post p;
            p.body(ss.str());
            p.send("api.github.com", "gists", 443);
            if(p.fail()) KEXCEPTION("Post JSON response failed to parse");
            for(const auto& a : p.json()) return a["id"].asString();
            KEXCEPTION("Post JSON resposne is empty");
        }
        static void DELETE(const std::string& id){}
};

}// END NAMESPACE git
}// END NAMESPACE kul



#endif /* _KUL_GIT_GIST_HPP_ */