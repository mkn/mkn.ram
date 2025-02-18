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
#ifndef _MKN_RAM_HTML_TAG_HPP_
#define _MKN_RAM_HTML_TAG_HPP_

#include "mkn/kul/except.hpp"
#include "mkn/kul/log.hpp"
#include "mkn/kul/map.hpp"
#include "mkn/kul/string.hpp"
#include "mkn/ram/html/def.hpp"

namespace mkn {
namespace ram {

class HTML {
 private:
  static void replace(std::string& s, std::string const& f, std::string const& r) {
    size_t p = s.find(f);
    while (p != std::string::npos) {
      s.replace(p, f.size(), r);
      p = s.find(f, p + r.size());
    }
  }

 public:
  static std::string& ESC(std::string& s) {
    replace(s, "&", "&amp;");
    replace(s, "<", "&lt;");
    replace(s, ">", "&gt;");
    replace(s, "\"", "&quot;");
    replace(s, "'", "&#x27;");
    replace(s, "/", "&#x2F;");
    return s;
  }
};

namespace html4 {

class Page;

class Tag {
 protected:
  std::string v = "";
  std::vector<std::pair<std::string, std::string>> atts;
  std::unique_ptr<std::string> str;
  std::vector<std::shared_ptr<Tag>> tags;
  virtual std::string const tag() const { return ""; }
  virtual std::string const& value() const { return v; }
  Tag() {}
  Tag(std::string const& v) : v(v) {}

  virtual std::string const* render(uint16_t tab = _MKN_RAM_HTML_FORMATED_) {
    std::stringstream ss;
#ifdef _MKN_RAM_HTML_FORMAT_
    ss << "\n";
    for (int i = 0; i < tab; i++) ss << "\t";
#endif /* _MKN_RAM_HTML_FORMAT_ */
    ss << "<" << tag();
    for (auto const& p : atts) {
      ss << " " << p.first;
      if (p.second.size()) ss << "=\"" << p.second << "\"";
    }
    if (tags.size() || v.size())
      ss << ">";
    else
      ss << "/>";
    ++tab;
    if (v.size()) ss << value();
    if (tags.size())
      for (auto const& t : tags)
        ss << *t->render(
#ifdef _MKN_RAM_HTML_FORMAT_
            tab
#endif /* _MKN_RAM_HTML_FORMAT_ */
        );
#ifdef _MKN_RAM_HTML_FORMAT_
    if (tags.size()) ss << "\n";
    if (tags.size())
      for (int i = 0; i < tab - 1; i++) ss << "\t";
#endif /* _MKN_RAM_HTML_FORMAT_ */
    if (tags.size() || v.size()) ss << "</" << tag() << ">";
    // #ifdef _MKN_RAM_HTML_FORMAT_
    // #endif /* _MKN_RAM_HTML_FORMAT_ */
    str = std::make_unique<std::string>(ss.str());
    return str.get();
  }

 public:
  Tag& attribute(std::string const& k, std::string const& v = "") {
    atts.push_back(std::make_pair(k, v));
    return *this;
  }
  Tag& add(std::shared_ptr<Tag> const& b) {
    tags.push_back(b);
    return *this;
  }
  size_t size() const { return tags.size(); }
  Tag& br();
  Tag& esc(std::string const& t);
  Tag& text(std::string const& t);
  friend class Page;
};

namespace tag {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

class Named : public Tag {
 private:
  std::string const n;

 protected:
  std::string const tag() const { return n; }

 public:
  Named(std::string const& n) : n(n) {}
  Named(std::string const& n, std::string const& v) : Tag(v), n(n) {}
};

class Label : public Tag {
 protected:
  std::string const tag() const { return "label"; }

 public:
  Label(std::string const& v) : Tag(v) {}
};

class InputTag : public Tag {
 protected:
  std::string const tag() const { return "input"; }
};

class TextBox : public InputTag {
 public:
  TextBox(std::string const& n) {
    attribute("name", n);
    attribute("type", "text");
  }
};

class TextArea : public Tag {};

class Select : public Tag {
 protected:
  std::string const tag() const { return "select"; }

 public:
  Select(std::string const& n) {
    attribute("id", n);
    attribute("name", n);
  }
  Select& option(std::string const& k, std::string const& v) {
    std::shared_ptr<Named> o = std::make_shared<Named>("option", v);
    o->attribute("value", k);
    add(o);
    return *this;
  }
};

class Radio : public Tag {};

class CheckBox : public InputTag {
 public:
  CheckBox(std::string const& n, std::string const& v, bool c = 0) {
    if (c) attribute("checked");
    attribute("name", n).attribute("value", v).attribute("type", "checkbox");
  }
};

class CheckList : public Tag {};

class Button : public InputTag {
 public:
  Button(std::string const& n, std::string const& v = "Submit", bool h = 0) {
    attribute("name", n).attribute("value", v).attribute("type", "submit");
    if (h)
      attribute("style", "position: absolute; left: -9999px; width: 1px; height: 1px;")
          .attribute("tabindex", "-1");
  }
};

enum FormMethod { POST = 0, GET };

class Form : public Tag {
 protected:
  FormMethod me;
  std::string const tag() const { return "form"; }

 public:
  Form(std::string const& n, FormMethod const& me = FormMethod::POST) : me(me) {
    if (me == FormMethod::POST)
      attribute("method", "post");
    else
      attribute("method", "get");
    attribute("name", n);
  }
  Form& button(std::string const& n, std::string const& v = "Submit", bool h = 0) {
    add(std::make_shared<Button>(n, v, h));
    return *this;
  }
  Form& hidden(std::string const& n, std::string const& v) {
    auto h = std::make_shared<Named>("input");
    h->attribute("type", "hidden");
    h->attribute("name", n);
    h->attribute("value", v);
    add(h);
    return *this;
  }
};

class TableRow : public Tag {
 protected:
  std::string const tag() const { return "tr"; }
};

class TableData : public Tag {
 protected:
  std::string const tag() const { return "td"; }

 public:
  TableData(std::string const& v = "") : Tag(v) {}
};

class Table;
class TableColumn : public Tag {
 protected:
  std::vector<std::shared_ptr<Tag>> tds;
  std::string const tag() const { return "th"; }

 public:
  TableColumn(std::string const& v = "") : Tag(v) {}
  TableColumn& data(std::string const& td) {
    tds.push_back(std::make_shared<TableData>(td));
    return *this;
  }
  TableColumn& data(std::shared_ptr<Tag> const& td) {
    tds.push_back(td);
    return *this;
  }
  friend class Table;
};

class Table : public Tag {
 private:
  bool sh = 1;
  std::vector<std::shared_ptr<TableColumn>> cols;

 protected:
  std::string const tag() const { return "table"; }

 public:
  Table(bool sh = 1) : sh(sh) {}
  TableColumn& column(std::string const& v = "") {
    auto tc = std::make_shared<TableColumn>(v);
    cols.push_back(tc);
    return *tc.get();
  }
  virtual std::string const* render(uint16_t tab = _MKN_RAM_HTML_FORMATED_) {
    if (sh) {
      std::shared_ptr<TableRow> row = std::make_shared<TableRow>();
      for (auto& h : cols) row->add(h);
      add(row);
    }
    if (cols.size())
      for (size_t i = 0; i < cols[0]->tds.size(); i++) {
        std::shared_ptr<TableRow> row = std::make_shared<TableRow>();
        for (auto const& c : cols)
          if (i < c->tds.size()) row->add(c->tds[i]);
        add(row);
      }
    return Tag::render(tab);
  }
};

}  // namespace tag

class Text : public Tag {
 public:
  Text(std::string const& n) : Tag(n) {}
  virtual std::string const* render(uint16_t tab = _MKN_RAM_HTML_FORMATED_) {
    std::stringstream ss;
#ifdef _MKN_RAM_HTML_FORMAT_
    ss << "\n";
    for (uint16_t i = 0; i < tab; i++) ss << "\t";
#endif /* _MKN_RAM_HTML_FORMAT_ */
    ss << v;
    str = std::make_unique<std::string>(ss.str());
    return str.get();
  }
};
namespace esc {
class Text : public mkn::ram::html4::Text {
 public:
  Text(std::string const& n) : mkn::ram::html4::Text(n) { mkn::ram::HTML::ESC(v); }
  virtual std::string const* render(uint16_t tab = _MKN_RAM_HTML_FORMATED_) {
    return mkn::ram::html4::Text::render(tab);
  }
};
}  // namespace esc

}  // namespace html4
}  // namespace ram
}  // namespace mkn

inline mkn::ram::html4::Tag& mkn::ram::html4::Tag::br() {
  tags.push_back(std::make_shared<tag::Named>("br"));
  return *this;
}
inline mkn::ram::html4::Tag& mkn::ram::html4::Tag::esc(std::string const& t) {
  tags.push_back(std::make_shared<esc::Text>(t));
  return *this;
}
inline mkn::ram::html4::Tag& mkn::ram::html4::Tag::text(std::string const& t) {
  tags.push_back(std::make_shared<Text>(t));
  return *this;
}
#endif /* _MKN_RAM_HTML_TAG_HPP_ */