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
// RFC 7540 - binary framing layer (frame header, known frame types, SETTINGS/GOAWAY/WINDOW_UPDATE
// payload (de)serialization). Pure, stateless, wire-format helpers only - no connection state.
#ifndef _MKN_RAM_HTTP2_FRAME_HPP_
#define _MKN_RAM_HTTP2_FRAME_HPP_

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace mkn {
namespace ram {
namespace http2 {
namespace frame {

enum class Type : uint8_t {
  DATA = 0x0,
  HEADERS = 0x1,
  PRIORITY = 0x2,
  RST_STREAM = 0x3,
  SETTINGS = 0x4,
  PUSH_PROMISE = 0x5,
  PING = 0x6,
  GOAWAY = 0x7,
  WINDOW_UPDATE = 0x8,
  CONTINUATION = 0x9,
};

namespace flag {
constexpr uint8_t END_STREAM = 0x1;
constexpr uint8_t ACK = 0x1;
constexpr uint8_t END_HEADERS = 0x4;
constexpr uint8_t PADDED = 0x8;
constexpr uint8_t PRIORITY = 0x20;
}  // namespace flag

// RFC 7540 3.5
inline std::string const& CONNECTION_PREFACE() {
  static std::string const s = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
  return s;
}
constexpr size_t HEADER_LEN = 9;

inline uint32_t READ_U24(uint8_t const* b) {
  return (uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | uint32_t(b[2]);
}
inline uint32_t READ_U31(uint8_t const* b) {
  return ((uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) |
          uint32_t(b[3])) &
         0x7FFFFFFFu;
}
inline uint32_t READ_U32(uint8_t const* b) {
  return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
}
inline void WRITE_U24(std::string& s, uint32_t v) {
  s.push_back(static_cast<char>((v >> 16) & 0xFF));
  s.push_back(static_cast<char>((v >> 8) & 0xFF));
  s.push_back(static_cast<char>(v & 0xFF));
}
inline void WRITE_U31(std::string& s, uint32_t v) {
  s.push_back(static_cast<char>((v >> 24) & 0x7F));
  s.push_back(static_cast<char>((v >> 16) & 0xFF));
  s.push_back(static_cast<char>((v >> 8) & 0xFF));
  s.push_back(static_cast<char>(v & 0xFF));
}

struct Header {
  uint32_t length = 0;  // 24-bit payload length
  Type type = Type::DATA;
  uint8_t flags = 0;
  uint32_t stream_id = 0;  // 31-bit, R bit masked off

  static Header PARSE(uint8_t const* buf) {
    Header h;
    h.length = READ_U24(buf);
    h.type = static_cast<Type>(buf[3]);
    h.flags = buf[4];
    h.stream_id = READ_U31(buf + 5);
    return h;
  }

  std::string toString() const {
    std::string s;
    s.reserve(HEADER_LEN);
    WRITE_U24(s, length);
    s.push_back(static_cast<char>(static_cast<uint8_t>(type)));
    s.push_back(static_cast<char>(flags));
    WRITE_U31(s, stream_id);
    return s;
  }
};

inline std::string BUILD(Type type, uint8_t flags, uint32_t stream_id,
                         std::string const& payload = "") {
  Header h;
  h.length = static_cast<uint32_t>(payload.size());
  h.type = type;
  h.flags = flags;
  h.stream_id = stream_id;
  return h.toString() + payload;
}

namespace settings {
constexpr uint16_t HEADER_TABLE_SIZE = 0x1;
constexpr uint16_t ENABLE_PUSH = 0x2;
constexpr uint16_t MAX_CONCURRENT_STREAMS = 0x3;
constexpr uint16_t INITIAL_WINDOW_SIZE = 0x4;
constexpr uint16_t MAX_FRAME_SIZE = 0x5;
constexpr uint16_t MAX_HEADER_LIST_SIZE = 0x6;

using Entry = std::pair<uint16_t, uint32_t>;

inline std::vector<Entry> PARSE(std::string const& payload) {
  std::vector<Entry> out;
  auto const* b = reinterpret_cast<uint8_t const*>(payload.data());
  size_t i = 0;
  for (; i + 6 <= payload.size(); i += 6) {
    uint16_t id = (uint16_t(b[i]) << 8) | uint16_t(b[i + 1]);
    uint32_t val = (uint32_t(b[i + 2]) << 24) | (uint32_t(b[i + 3]) << 16) |
                   (uint32_t(b[i + 4]) << 8) | uint32_t(b[i + 5]);
    out.emplace_back(id, val);
  }
  return out;
}

inline std::string BUILD(std::vector<Entry> const& entries) {
  std::string payload;
  payload.reserve(entries.size() * 6);
  for (auto const& e : entries) {
    payload.push_back(static_cast<char>((e.first >> 8) & 0xFF));
    payload.push_back(static_cast<char>(e.first & 0xFF));
    payload.push_back(static_cast<char>((e.second >> 24) & 0xFF));
    payload.push_back(static_cast<char>((e.second >> 16) & 0xFF));
    payload.push_back(static_cast<char>((e.second >> 8) & 0xFF));
    payload.push_back(static_cast<char>(e.second & 0xFF));
  }
  return payload;
}
}  // namespace settings

struct GoAway {
  uint32_t last_stream_id = 0;
  uint32_t error_code = 0;
  std::string debug;

  static GoAway PARSE(std::string const& payload) {
    GoAway g;
    if (payload.size() < 8) return g;
    auto const* b = reinterpret_cast<uint8_t const*>(payload.data());
    g.last_stream_id = READ_U31(b);
    g.error_code = READ_U32(b + 4);
    g.debug = payload.substr(8);
    return g;
  }
};

inline uint32_t WINDOW_UPDATE_INCREMENT(std::string const& payload) {
  if (payload.size() < 4) return 0;
  return READ_U31(reinterpret_cast<uint8_t const*>(payload.data()));
}

inline std::string WINDOW_UPDATE_PAYLOAD(uint32_t increment) {
  std::string s;
  WRITE_U31(s, increment);
  return s;
}

}  // namespace frame
}  // namespace http2
}  // namespace ram
}  // namespace mkn

#endif /* _MKN_RAM_HTTP2_FRAME_HPP_ */
