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
#include "mkn/ram/http2/hpack.hpp"

#include <memory>

namespace mkn {
namespace ram {
namespace http2 {
namespace hpack {

uint64_t Integer::decode(uint8_t const* buf, size_t len, size_t& pos, uint8_t prefixBits) {
  if (pos >= len) KEXCEPTION("HPACK: truncated integer");
  uint8_t const mask = static_cast<uint8_t>((1u << prefixBits) - 1);
  uint64_t value = buf[pos] & mask;
  pos++;
  if (value < mask) return value;
  uint64_t m = 0;
  uint8_t b;
  do {
    if (pos >= len) KEXCEPTION("HPACK: truncated integer");
    b = buf[pos++];
    value += static_cast<uint64_t>(b & 0x7F) << m;
    m += 7;
  } while (b & 0x80);
  return value;
}

std::string Integer::encode(uint64_t value, uint8_t prefixBits, uint8_t firstByteFlags) {
  std::string out;
  uint8_t const maxPrefix = static_cast<uint8_t>((1u << prefixBits) - 1);
  if (value < maxPrefix) {
    out.push_back(static_cast<char>(firstByteFlags | static_cast<uint8_t>(value)));
    return out;
  }
  out.push_back(static_cast<char>(firstByteFlags | maxPrefix));
  uint64_t remaining = value - maxPrefix;
  while (remaining >= 128) {
    out.push_back(static_cast<char>((remaining % 128) | 0x80));
    remaining /= 128;
  }
  out.push_back(static_cast<char>(remaining));
  return out;
}

namespace {

// RFC 7541 Appendix B - Huffman code for string literals, {code (LSB-aligned), bit length},
// indexed by symbol 0..255. Mechanically extracted from the canonical RFC text (not
// hand-transcribed) to guarantee bit-exact correctness against real HTTP/2 servers.
std::pair<uint32_t, uint8_t> const HUFFMAN_TABLE[256] = {
    {0x1ff8, 13},     {0x7fffd8, 23},  {0xfffffe2, 28},  {0xfffffe3, 28},  {0xfffffe4, 28},
    {0xfffffe5, 28},  {0xfffffe6, 28}, {0xfffffe7, 28},  {0xfffffe8, 28},  {0xffffea, 24},
    {0x3ffffffc, 30}, {0xfffffe9, 28}, {0xfffffea, 28},  {0x3ffffffd, 30}, {0xfffffeb, 28},
    {0xfffffec, 28},  {0xfffffed, 28}, {0xfffffee, 28},  {0xfffffef, 28},  {0xffffff0, 28},
    {0xffffff1, 28},  {0xffffff2, 28}, {0x3ffffffe, 30}, {0xffffff3, 28},  {0xffffff4, 28},
    {0xffffff5, 28},  {0xffffff6, 28}, {0xffffff7, 28},  {0xffffff8, 28},  {0xffffff9, 28},
    {0xffffffa, 28},  {0xffffffb, 28}, {0x14, 6},        {0x3f8, 10},      {0x3f9, 10},
    {0xffa, 12},      {0x1ff9, 13},    {0x15, 6},        {0xf8, 8},        {0x7fa, 11},
    {0x3fa, 10},      {0x3fb, 10},     {0xf9, 8},        {0x7fb, 11},      {0xfa, 8},
    {0x16, 6},        {0x17, 6},       {0x18, 6},        {0x0, 5},         {0x1, 5},
    {0x2, 5},         {0x19, 6},       {0x1a, 6},        {0x1b, 6},        {0x1c, 6},
    {0x1d, 6},        {0x1e, 6},       {0x1f, 6},        {0x5c, 7},        {0xfb, 8},
    {0x7ffc, 15},     {0x20, 6},       {0xffb, 12},      {0x3fc, 10},      {0x1ffa, 13},
    {0x21, 6},        {0x5d, 7},       {0x5e, 7},        {0x5f, 7},        {0x60, 7},
    {0x61, 7},        {0x62, 7},       {0x63, 7},        {0x64, 7},        {0x65, 7},
    {0x66, 7},        {0x67, 7},       {0x68, 7},        {0x69, 7},        {0x6a, 7},
    {0x6b, 7},        {0x6c, 7},       {0x6d, 7},        {0x6e, 7},        {0x6f, 7},
    {0x70, 7},        {0x71, 7},       {0x72, 7},        {0xfc, 8},        {0x73, 7},
    {0xfd, 8},        {0x1ffb, 13},    {0x7fff0, 19},    {0x1ffc, 13},     {0x3ffc, 14},
    {0x22, 6},        {0x7ffd, 15},    {0x3, 5},         {0x23, 6},        {0x4, 5},
    {0x24, 6},        {0x5, 5},        {0x25, 6},        {0x26, 6},        {0x27, 6},
    {0x6, 5},         {0x74, 7},       {0x75, 7},        {0x28, 6},        {0x29, 6},
    {0x2a, 6},        {0x7, 5},        {0x2b, 6},        {0x76, 7},        {0x2c, 6},
    {0x8, 5},         {0x9, 5},        {0x2d, 6},        {0x77, 7},        {0x78, 7},
    {0x79, 7},        {0x7a, 7},       {0x7b, 7},        {0x7ffe, 15},     {0x7fc, 11},
    {0x3ffd, 14},     {0x1ffd, 13},    {0xffffffc, 28},  {0xfffe6, 20},    {0x3fffd2, 22},
    {0xfffe7, 20},    {0xfffe8, 20},   {0x3fffd3, 22},   {0x3fffd4, 22},   {0x3fffd5, 22},
    {0x7fffd9, 23},   {0x3fffd6, 22},  {0x7fffda, 23},   {0x7fffdb, 23},   {0x7fffdc, 23},
    {0x7fffdd, 23},   {0x7fffde, 23},  {0xffffeb, 24},   {0x7fffdf, 23},   {0xffffec, 24},
    {0xffffed, 24},   {0x3fffd7, 22},  {0x7fffe0, 23},   {0xffffee, 24},   {0x7fffe1, 23},
    {0x7fffe2, 23},   {0x7fffe3, 23},  {0x7fffe4, 23},   {0x1fffdc, 21},   {0x3fffd8, 22},
    {0x7fffe5, 23},   {0x3fffd9, 22},  {0x7fffe6, 23},   {0x7fffe7, 23},   {0xffffef, 24},
    {0x3fffda, 22},   {0x1fffdd, 21},  {0xfffe9, 20},    {0x3fffdb, 22},   {0x3fffdc, 22},
    {0x7fffe8, 23},   {0x7fffe9, 23},  {0x1fffde, 21},   {0x7fffea, 23},   {0x3fffdd, 22},
    {0x3fffde, 22},   {0xfffff0, 24},  {0x1fffdf, 21},   {0x3fffdf, 22},   {0x7fffeb, 23},
    {0x7fffec, 23},   {0x1fffe0, 21},  {0x1fffe1, 21},   {0x3fffe0, 22},   {0x1fffe2, 21},
    {0x7fffed, 23},   {0x3fffe1, 22},  {0x7fffee, 23},   {0x7fffef, 23},   {0xfffea, 20},
    {0x3fffe2, 22},   {0x3fffe3, 22},  {0x3fffe4, 22},   {0x7ffff0, 23},   {0x3fffe5, 22},
    {0x3fffe6, 22},   {0x7ffff1, 23},  {0x3ffffe0, 26},  {0x3ffffe1, 26},  {0xfffeb, 20},
    {0x7fff1, 19},    {0x3fffe7, 22},  {0x7ffff2, 23},   {0x3fffe8, 22},   {0x1ffffec, 25},
    {0x3ffffe2, 26},  {0x3ffffe3, 26}, {0x3ffffe4, 26},  {0x7ffffde, 27},  {0x7ffffdf, 27},
    {0x3ffffe5, 26},  {0xfffff1, 24},  {0x1ffffed, 25},  {0x7fff2, 19},    {0x1fffe3, 21},
    {0x3ffffe6, 26},  {0x7ffffe0, 27}, {0x7ffffe1, 27},  {0x3ffffe7, 26},  {0x7ffffe2, 27},
    {0xfffff2, 24},   {0x1fffe4, 21},  {0x1fffe5, 21},   {0x3ffffe8, 26},  {0x3ffffe9, 26},
    {0xffffffd, 28},  {0x7ffffe3, 27}, {0x7ffffe4, 27},  {0x7ffffe5, 27},  {0xfffec, 20},
    {0xfffff3, 24},   {0xfffed, 20},   {0x1fffe6, 21},   {0x3fffe9, 22},   {0x1fffe7, 21},
    {0x1fffe8, 21},   {0x7ffff3, 23},  {0x3fffea, 22},   {0x3fffeb, 22},   {0x1ffffee, 25},
    {0x1ffffef, 25},  {0xfffff4, 24},  {0xfffff5, 24},   {0x3ffffea, 26},  {0x7ffff4, 23},
    {0x3ffffeb, 26},  {0x7ffffe6, 27}, {0x3ffffec, 26},  {0x3ffffed, 26},  {0x7ffffe7, 27},
    {0x7ffffe8, 27},  {0x7ffffe9, 27}, {0x7ffffea, 27},  {0x7ffffeb, 27},  {0xffffffe, 28},
    {0x7ffffec, 27},  {0x7ffffed, 27}, {0x7ffffee, 27},  {0x7ffffef, 27},  {0x7fffff0, 27},
    {0x3ffffee, 26},
};

struct HuffmanNode {
  int symbol = -1;
  std::unique_ptr<HuffmanNode> child[2];
};

HuffmanNode const& huffmanRoot() {
  static HuffmanNode const root = [] {
    HuffmanNode r;
    for (int sym = 0; sym < 256; sym++) {
      uint32_t const code = HUFFMAN_TABLE[sym].first;
      uint8_t const length = HUFFMAN_TABLE[sym].second;
      HuffmanNode* n = &r;
      for (int b = length - 1; b >= 0; b--) {
        uint8_t const bit = (code >> b) & 1;
        if (!n->child[bit]) n->child[bit] = std::make_unique<HuffmanNode>();
        n = n->child[bit].get();
      }
      n->symbol = sym;
    }
    return r;
  }();
  return root;
}

}  // namespace

std::string Huffman::decode(uint8_t const* buf, size_t len) {
  std::string out;
  HuffmanNode const& root = huffmanRoot();
  HuffmanNode const* cur = &root;
  uint32_t partial = 0;
  uint8_t partialLen = 0;
  for (size_t i = 0; i < len; i++) {
    uint8_t const byte = buf[i];
    for (int b = 7; b >= 0; b--) {
      uint8_t const bit = (byte >> b) & 1;
      HuffmanNode const* next = cur->child[bit].get();
      if (!next) KEXCEPTION("HPACK: invalid Huffman code");
      cur = next;
      partial = (partial << 1) | bit;
      partialLen++;
      if (cur->symbol >= 0) {
        out.push_back(static_cast<char>(cur->symbol));
        cur = &root;
        partial = 0;
        partialLen = 0;
      }
    }
  }
  if (partialLen > 0) {
    uint32_t const allOnes = (1u << partialLen) - 1;
    if (partialLen > 7 || partial != allOnes) KEXCEPTION("HPACK: invalid Huffman padding");
  }
  return out;
}

namespace {
// RFC 7541 Appendix A - the 61 static table entries, in index order (index = position + 1)
Header const STATIC_TABLE_DATA[] = {
    {":authority", ""},
    {":method", "GET"},
    {":method", "POST"},
    {":path", "/"},
    {":path", "/index.html"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "200"},
    {":status", "204"},
    {":status", "206"},
    {":status", "304"},
    {":status", "400"},
    {":status", "404"},
    {":status", "500"},
    {"accept-charset", ""},
    {"accept-encoding", "gzip, deflate"},
    {"accept-language", ""},
    {"accept-ranges", ""},
    {"accept", ""},
    {"access-control-allow-origin", ""},
    {"age", ""},
    {"allow", ""},
    {"authorization", ""},
    {"cache-control", ""},
    {"content-disposition", ""},
    {"content-encoding", ""},
    {"content-language", ""},
    {"content-length", ""},
    {"content-location", ""},
    {"content-range", ""},
    {"content-type", ""},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"expect", ""},
    {"expires", ""},
    {"from", ""},
    {"host", ""},
    {"if-match", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"if-range", ""},
    {"if-unmodified-since", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"max-forwards", ""},
    {"proxy-authenticate", ""},
    {"proxy-authorization", ""},
    {"range", ""},
    {"referer", ""},
    {"refresh", ""},
    {"retry-after", ""},
    {"server", ""},
    {"set-cookie", ""},
    {"strict-transport-security", ""},
    {"transfer-encoding", ""},
    {"user-agent", ""},
    {"vary", ""},
    {"via", ""},
    {"www-authenticate", ""},
};
constexpr size_t STATIC_TABLE_SIZE = sizeof(STATIC_TABLE_DATA) / sizeof(STATIC_TABLE_DATA[0]);
}  // namespace

Header const& StaticTable::AT(size_t index) {
  if (index < 1 || index > STATIC_TABLE_SIZE) KEXCEPTION("HPACK: static table index out of range");
  return STATIC_TABLE_DATA[index - 1];
}

size_t StaticTable::SIZE() { return STATIC_TABLE_SIZE; }

size_t StaticTable::FIND(std::string const& name, std::string const& value) {
  for (size_t i = 0; i < STATIC_TABLE_SIZE; i++)
    if (STATIC_TABLE_DATA[i].first == name && STATIC_TABLE_DATA[i].second == value) return i + 1;
  return 0;
}

size_t StaticTable::FIND_NAME(std::string const& name) {
  for (size_t i = 0; i < STATIC_TABLE_SIZE; i++)
    if (STATIC_TABLE_DATA[i].first == name) return i + 1;
  return 0;
}

void DynamicTable::add(std::string const& name, std::string const& value) {
  size_t const entrySize = 32 + name.size() + value.size();
  if (entrySize > _max) {
    // RFC 7541 4.4 - doesn't fit even after evicting everything -> table ends up empty
    _entries.clear();
    _size = 0;
    return;
  }
  evict();
  while (_size + entrySize > _max && !_entries.empty()) {
    auto const& back = _entries.back();
    _size -= 32 + back.first.size() + back.second.size();
    _entries.pop_back();
  }
  _entries.emplace_front(name, value);
  _size += entrySize;
}

void DynamicTable::resize(size_t maxSize) {
  _max = maxSize;
  evict();
}

void DynamicTable::evict() {
  while (_size > _max && !_entries.empty()) {
    auto const& back = _entries.back();
    _size -= 32 + back.first.size() + back.second.size();
    _entries.pop_back();
  }
}

Headers Decoder::decode(std::string const& block) {
  Headers out;
  auto const* buf = reinterpret_cast<uint8_t const*>(block.data());
  size_t const len = block.size();
  size_t pos = 0;

  auto readString = [&]() -> std::string {
    if (pos >= len) KEXCEPTION("HPACK: truncated string literal");
    bool const huff = (buf[pos] & 0x80) != 0;
    uint64_t const slen = Integer::decode(buf, len, pos, 7);
    if (pos + slen > len) KEXCEPTION("HPACK: truncated string literal");
    std::string s = huff ? Huffman::decode(buf + pos, static_cast<size_t>(slen))
                         : std::string(reinterpret_cast<char const*>(buf + pos), slen);
    pos += slen;
    return s;
  };

  auto lookup = [&](uint64_t index) -> Header const& {
    if (index == 0) KEXCEPTION("HPACK: zero index");
    if (index <= StaticTable::SIZE()) return StaticTable::AT(static_cast<size_t>(index));
    size_t const dynIdx = static_cast<size_t>(index) - StaticTable::SIZE() - 1;
    if (dynIdx >= _table.count()) KEXCEPTION("HPACK: dynamic table index out of range");
    return _table.at(dynIdx);
  };

  while (pos < len) {
    uint8_t const first = buf[pos];
    if (first & 0x80) {
      // Indexed Header Field - RFC 7541 6.1
      uint64_t const index = Integer::decode(buf, len, pos, 7);
      out.push_back(lookup(index));
    } else if (first & 0x40) {
      // Literal Header Field with Incremental Indexing - RFC 7541 6.2.1
      uint64_t const nameIndex = Integer::decode(buf, len, pos, 6);
      std::string name = nameIndex ? lookup(nameIndex).first : readString();
      std::string value = readString();
      _table.add(name, value);
      out.emplace_back(std::move(name), std::move(value));
    } else if (first & 0x20) {
      // Dynamic Table Size Update - RFC 7541 6.3
      uint64_t const newSize = Integer::decode(buf, len, pos, 5);
      _table.resize(static_cast<size_t>(newSize));
    } else {
      // Literal Header Field without Indexing (6.2.2) or Never Indexed (6.2.3) -
      // identical parsing (both 4-bit prefix, neither updates the dynamic table)
      uint64_t const nameIndex = Integer::decode(buf, len, pos, 4);
      std::string name = nameIndex ? lookup(nameIndex).first : readString();
      std::string value = readString();
      out.emplace_back(std::move(name), std::move(value));
    }
  }
  return out;
}

std::string Encoder::encode(Headers const& headers) {
  std::string out;
  for (auto const& h : headers) {
    size_t const nameIndex = StaticTable::FIND_NAME(h.first);
    if (nameIndex) {
      // Literal Header Field without Indexing, indexed name (RFC 7541 6.2.2)
      out += Integer::encode(nameIndex, 4, 0x00);
    } else {
      out += Integer::encode(0, 4, 0x00);
      out += Integer::encode(h.first.size(), 7, 0x00);  // H=0 (no Huffman on encode)
      out += h.first;
    }
    out += Integer::encode(h.second.size(), 7, 0x00);
    out += h.second;
  }
  return out;
}

}  // namespace hpack
}  // namespace http2
}  // namespace ram
}  // namespace mkn
