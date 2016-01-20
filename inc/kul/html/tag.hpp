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
#ifndef _KUL_HTML_TAG_HPP_
#define _KUL_HTML_TAG_HPP_

#include "kul/log.hpp"
#include "kul/map.hpp"
#include "kul/except.hpp"
#include "kul/string.hpp"

#include "kul/html/def.hpp"

namespace kul{ 

class HTML{
	private:
		static void replace(std::string& s, const std::string& f, const std::string& r){
			size_t p = s.find(f);
			while(p != std::string::npos){
				s.replace(p, f.size(), r);
				p = s.find(f, p + r.size()) ;
			}
		}
	public:
		static std::string& ESC(std::string& s){
			replace(s, "&" , "&amp;");
			replace(s, "<" , "&lt;");
			replace(s, ">" , "&gt;");
			replace(s, "\"", "&quot;");
			replace(s, "'" , "&#x27;");
			replace(s, "/" , "&#x2F;");
			return s;
		}
};

namespace html4{

class Page;

class Tag{
	protected:
		std::string v = "";
		std::vector<std::pair<std::string, std::string>> atts;
		std::unique_ptr<std::string> str;
		std::vector<std::shared_ptr<Tag>> tags;
		virtual const std::string tag() const    { return ""; }
		virtual const std::string& value() const { return v; }
		Tag(){}
		Tag(const std::string& v) : v(v){}
		virtual const std::string* render(){
			uint16_t t = 1;
			return render(t);
		}
		virtual const std::string* render(uint16_t tab){
			std::stringstream ss;
#ifdef _KUL_HTML_FORMATED_
			ss << "\n";
			for(int i = 0; i < tab; i++) ss << "\t";
#endif /* _KUL_HTML_FORMATED_ */
			ss << "<" << tag();
			for(const auto& p : atts) ss << " " << p.first << "=\"" << p.second << "\"";
			if(tags.size() || v.size())
				ss << ">";
			else
				ss << "/>";
			++tab;
			if(v.size()) ss << value();
			if(tags.size())
				for(const auto& t : tags)
					ss << *t->render(
#ifdef _KUL_HTML_FORMATED_
						tab
#endif /* _KUL_HTML_FORMATED_ */
						);
#ifdef _KUL_HTML_FORMATED_
			if(tags.size()) for(int i = 0; i < tab - 1; i++) ss << "\t";
			if(tags.size() || v.size()) ss << "</" << tag() << ">" << "\n";
#else
			if(tags.size() || v.size()) ss << "</" << tag() << ">";
#endif /* _KUL_HTML_FORMATED_ */
			str = std::make_unique<std::string>(ss.str());
			return str.get();
		}
	public:
		Tag& attribute(const std::string& k, const std::string& v){
			atts.push_back(std::make_pair(k, v));
			return *this;
		}
		Tag& tag(const std::shared_ptr<Tag>& b){
			tags.push_back(b);
			return *this;
		}
		Tag& br();
		Tag& esc(const std::string& t);
		Tag& text(const std::string& t);
		friend class Page;
};

namespace tag{

class Exception : public kul::Exception{
	public:
		Exception(const char*f, const uint16_t& l, const std::string& s) : kul::Exception(f, l, s){}
};

class Named : public Tag{
	private:
		const std::string n;
	protected:
		const std::string tag() const { return n; }
	public:
		Named(const std::string& n) : n(n){}
		Named(const std::string& n, const std::string& v) : Tag(v), n(n){}
		void value(const std::string& s) { this->v = s; }
};

class Label : public Tag{
	protected:
		const std::string tag() const   { return "label"; }
	public:
		Label(const std::string& v) : Tag(v){}
};

class TextBox : public Tag{}; 

class TextArea : public Tag{};

class DropDown : public Tag{};

class Radio : public Tag{};

class CheckBox : public Tag{};

class CheckList : public Tag{};

}// END NAMESPACE tag

class Text : public Tag{
	public:
		Text(const std::string& n) : Tag(n){}
		virtual const std::string* render(uint16_t tab){
			std::stringstream ss;
#ifdef _KUL_HTML_FORMATED_
			ss << "\n";
			for(uint16_t i = 0; i < tab; i++) ss << "\t";
#endif /* _KUL_HTML_FORMATED_ */
			ss << v;
			str = std::make_unique<std::string>(ss.str());
			return str.get();
		}
};
namespace esc{
class Text : public kul::html4::Text{
	public:
		Text(const std::string& n) : kul::html4::Text(n){
			kul::HTML::ESC(v);
		}
		virtual const std::string* render(uint16_t tab){
			return kul::html4::Text::render(tab);
		}
};
} 

}// END NAMESPACE html4
}// END NAMESPACE kul

inline kul::html4::Tag& kul::html4::Tag::br(){
	tags.push_back(std::make_shared<tag::Named>("br"));
	return *this;
}
inline kul::html4::Tag& kul::html4::Tag::esc(const std::string& t){
	tags.push_back(std::make_shared<esc::Text>(t));
	return *this;
}
inline kul::html4::Tag& kul::html4::Tag::text(const std::string& t){
	tags.push_back(std::make_shared<Text>(t));
	return *this;
}
#endif /* _KUL_HTML_TAG_HPP_ */