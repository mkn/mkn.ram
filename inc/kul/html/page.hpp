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
#ifndef _KUL_HTML_PAGE_HPP_
#define _KUL_HTML_PAGE_HPP_

#include "kul/html/tag.hpp"

namespace kul {
namespace html4 {

namespace tag {
class Body : public Tag {
 protected:
  virtual const std::string tag() const { return "body"; }
};
class Head : public Tag {
 protected:
  virtual const std::string tag() const { return "head"; }
};
}  // namespace tag

class Page {
 protected:
  size_t p = 0;
  uint16_t c = -1;
  const std::string t = "", x = "";
  std::shared_ptr<const std::string> str;
  std::shared_ptr<tag::Head> h;
  std::shared_ptr<tag::Body> b;
  Page() : h(std::make_shared<tag::Head>()), b(std::make_shared<tag::Body>()) {}

 public:
  Tag& body() { return *b.get(); }
  Page& body(const std::shared_ptr<tag::Body>& t) {
    b = t;
    return *this;
  }
  template <class T>
  Page& body(const std::shared_ptr<T>& t) {
    b->tags.push_back(t);
    return *this;
  }
  Tag& head() { return *h.get(); }
  Page& head(const std::shared_ptr<tag::Head>& t) {
    h = t;
    return *this;
  }
  template <class T>
  Page& head(const std::shared_ptr<T>& t) {
    h->tags.push_back(t);
    return *this;
  }
  virtual const std::string* render() {
    std::stringstream ss;
#if defined(_KUL_HTML_DOC_TYPE_)
    ss << _KUL_HTML_DOC_TYPE_ << "\n";
#endif
    ss << "<html>";
    ss << *h->render();
    ss << *b->render();
    ss << x << "\n</html>";
    str = std::make_shared<std::string>(ss.str());
    return str.get();
  }
};

}  // END NAMESPACE html4
}  // END NAMESPACE kul

#endif /* _KUL_HTML_PAGE_HPP_ */