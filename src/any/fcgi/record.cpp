/**
Copyright (c) 2026, Philip Deegan.
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
#include "mkn/ram/fcgi.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace mkn::ram::fcgi {
namespace {

void header(std::string& out, std::uint8_t type, std::uint16_t id, std::size_t length,
            std::uint8_t padding) {
  char const h[HEADER_LEN] = {static_cast<char>(VERSION_1),   static_cast<char>(type),
                              static_cast<char>(id >> 8),     static_cast<char>(id & 0xff),
                              static_cast<char>(length >> 8), static_cast<char>(length & 0xff),
                              static_cast<char>(padding),     0};
  out.append(h, HEADER_LEN);
}

void length(std::string& out, std::size_t const n) {
  if (n < 128)
    out += static_cast<char>(n);
  else {
    out += static_cast<char>(((n >> 24) & 0x7f) | 0x80);
    out += static_cast<char>((n >> 16) & 0xff);
    out += static_cast<char>((n >> 8) & 0xff);
    out += static_cast<char>(n & 0xff);
  }
}

std::size_t length(std::string_view const in, std::size_t& pos) {
  if (pos >= in.size()) KEXCEPT(Exception, "fcgi name-value pair truncated");
  auto const b = [&](std::size_t i) { return static_cast<std::uint8_t>(in[i]); };
  if (!(b(pos) & 0x80)) return b(pos++);
  if (pos + 4 > in.size()) KEXCEPT(Exception, "fcgi name-value pair truncated");
  std::size_t const n =
      (std::size_t(b(pos) & 0x7f) << 24) | (b(pos + 1) << 16) | (b(pos + 2) << 8) | b(pos + 3);
  pos += 4;
  return n;
}

bool iequals(std::string_view const a, std::string_view const b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) ==
           std::tolower(static_cast<unsigned char>(y));
  });
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

// HTTP_ACCEPT_ENCODING -> Accept-Encoding
std::string headerName(std::string_view const cgi) {
  std::string name;
  bool upper = true;
  for (char const c : cgi) {
    if (c == '_') {
      name += '-';
      upper = true;
    } else {
      auto const u = static_cast<unsigned char>(c);
      name += static_cast<char>(upper ? std::toupper(u) : std::tolower(u));
      upper = false;
    }
  }
  return name;
}

template <class F>
void split(std::string_view s, char const sep, F&& f) {
  while (!s.empty()) {
    auto const at = s.find(sep);
    f(s.substr(0, at));
    if (at == std::string_view::npos) break;
    s.remove_prefix(at + 1);
  }
}

std::string get(Params const& params, std::string const& key) {
  auto const it = params.find(key);
  return it == params.end() ? "" : it->second;
}

std::string uri(Params const& params) {
  if (auto u = get(params, "REQUEST_URI"); !u.empty()) return u;
  auto u = get(params, "SCRIPT_NAME") + get(params, "PATH_INFO");
  if (auto const q = get(params, "QUERY_STRING"); !q.empty()) u += "?" + q;
  return u;
}

std::string cgiPath(Params const& params) {
  auto const u = uri(params);
  return u.substr(0, u.find('?'));
}

std::string cgiHost(Params const& params) {
  if (auto h = get(params, "HTTP_HOST"); !h.empty()) return h;
  return get(params, "SERVER_NAME");
}

std::uint16_t cgiPort(Params const& params) {
  auto const p = get(params, "REMOTE_PORT");
  std::uint16_t port = 0;
  std::from_chars(p.data(), p.data() + p.size(), port);
  return port;
}

}  // namespace

void record::append(std::string& out, std::uint8_t const type, std::uint16_t const id,
                    std::string_view content) {
  do {
    auto const n = (std::min)(content.size(), MAX_CONTENT);
    auto const pad = static_cast<std::uint8_t>((8 - n % 8) % 8);
    header(out, type, id, n, pad);
    out.append(content.data(), n);
    out.append(pad, '\0');
    content.remove_prefix(n);
  } while (!content.empty());
}

void record::begin(std::string& out, std::uint16_t const id, std::uint16_t const role,
                   std::uint8_t const flags) {
  char const body[8] = {static_cast<char>(role >> 8),
                        static_cast<char>(role & 0xff),
                        static_cast<char>(flags),
                        0,
                        0,
                        0,
                        0,
                        0};
  append(out, type::BEGIN_REQUEST, id, {body, sizeof(body)});
}

void record::end(std::string& out, std::uint16_t const id, std::uint8_t const protocolStatus,
                 std::uint32_t const appStatus) {
  char const body[8] = {static_cast<char>(appStatus >> 24),
                        static_cast<char>((appStatus >> 16) & 0xff),
                        static_cast<char>((appStatus >> 8) & 0xff),
                        static_cast<char>(appStatus & 0xff),
                        static_cast<char>(protocolStatus),
                        0,
                        0,
                        0};
  append(out, type::END_REQUEST, id, {body, sizeof(body)});
}

std::string record::encode(Params const& params) {
  std::string out;
  for (auto const& [k, v] : params) {
    length(out, k.size());
    length(out, v.size());
    out += k;
    out += v;
  }
  return out;
}

Params record::decode(std::string_view const in) {
  Params params;
  std::size_t pos = 0;
  while (pos < in.size()) {
    auto const k = length(in, pos);
    auto const v = length(in, pos);
    if (k > in.size() - pos || v > in.size() - pos - k)
      KEXCEPT(Exception, "fcgi name-value pair truncated");
    params.insert_or_assign(std::string(in.substr(pos, k)), std::string(in.substr(pos + k, v)));
    pos += k + v;
  }
  return params;
}

std::string record::cgi(http::Response const& response) {
  std::string out =
      "Status: " + std::to_string(response.status()) + " " + response.reason() + "\r\n";
  for (auto const& [k, v] : response.headers()) {
    // the web server owns the client connection
    if (iequals(k, "Status") || iequals(k, "Connection") || iequals(k, "Keep-Alive") ||
        iequals(k, "Transfer-Encoding"))
      continue;
    out += k + ": " + v + "\r\n";
  }
  for (auto const& p : response.cookies())
    out += "Set-Cookie: " + http::Response::SET_COOKIE(p.first, p.second) + "\r\n";
  out += "\r\n";
  out += response.body();
  return out;
}

std::string record::respond(std::uint16_t const id, http::Response const& response) {
  std::string out;
  append(out, type::STDOUT, id, cgi(response));
  append(out, type::STDOUT, id, {});
  end(out, id, status::REQUEST_COMPLETE);
  return out;
}

bool record::Parser::next(Record& record) {
  auto const available = _buf.size() - _pos;
  if (available >= HEADER_LEN) {
    auto const b = [&](std::size_t i) { return static_cast<std::uint8_t>(_buf[_pos + i]); };
    if (b(0) != VERSION_1) KEXCEPT(Exception, "fcgi unsupported version: " + std::to_string(b(0)));
    std::size_t const n = (b(4) << 8) | b(5), total = HEADER_LEN + n + b(6);
    if (available >= total) {
      record.type = b(1);
      record.id = static_cast<std::uint16_t>((b(2) << 8) | b(3));
      record.content.assign(_buf, _pos + HEADER_LEN, n);
      _pos += total;
      return true;
    }
  }
  _buf.erase(0, _pos);
  _pos = 0;
  return false;
}

Request::Request(Params params, std::string b)
    : http::A1_1Request(cgiHost(params), cgiPath(params), cgiPort(params),
                        get(params, "REMOTE_ADDR")),
      _method(get(params, "REQUEST_METHOD")),
      _uri(uri(params)),
      _params(std::move(params)) {
  if (_method.empty()) _method = "GET";
  for (auto const& [k, v] : _params) {
    if (k == "HTTP_COOKIE") {
      split(v, ';', [&](std::string_view c) {
        c = trim(c);
        if (c.empty()) return;
        auto const eq = c.find('=');
        cookie(std::string(c.substr(0, eq)),
               eq == std::string_view::npos ? "" : std::string(c.substr(eq + 1)));
      });
    } else if (k.starts_with("HTTP_"))
      header(headerName(std::string_view(k).substr(5)), v);
    else if ((k == "CONTENT_TYPE" || k == "CONTENT_LENGTH") && !v.empty())
      header(headerName(k), v);
  }
  split(get(_params, "QUERY_STRING"), '&', [&](std::string_view a) {
    if (a.empty()) return;
    auto const eq = a.find('=');
    attribute(std::string(a.substr(0, eq)),
              eq == std::string_view::npos ? "" : std::string(a.substr(eq + 1)));
  });
  body(std::move(b));
}

std::string Request::toString() const {
  std::stringstream ss;
  ss << _method << " " << _uri << " " << version() << "\r\n";
  for (auto const& [k, v] : headers()) ss << k << ": " << v << "\r\n";
  if (!cookies().empty()) {
    ss << "Cookie: ";
    bool first = true;
    for (auto const& p : cookies()) {
      ss << (first ? "" : "; ") << p.first << "=" << p.second;
      first = false;
    }
    ss << "\r\n";
  }
  ss << "\r\n" << body();
  return ss.str();
}

}  // namespace mkn::ram::fcgi
