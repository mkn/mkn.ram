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
// RFC 7541 - HPACK header compression, as used by HTTP/2's HEADERS/CONTINUATION frames.
//
// This is a client-focused implementation: the Encoder only ever emits literal
// representations (RFC 7541 6.2.2, "Literal Header Field without Indexing" with an
// indexed name where possible) - correct per spec but intentionally not optimal, since
// the request side doesn't need dynamic-table bookkeeping to interoperate. The Decoder
// is a full implementation (indexed fields, all three literal representations, dynamic
// table with eviction, Huffman-coded strings) since real servers use all of these when
// encoding responses.
#ifndef _MKN_RAM_HTTP2_HPACK_HPP_
#define _MKN_RAM_HTTP2_HPACK_HPP_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "mkn/ram/http2/def.hpp"
#include "mkn/kul/except.hpp"

namespace mkn {
namespace ram {
namespace http2 {
namespace hpack {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

using Header = std::pair<std::string, std::string>;
using Headers = std::vector<Header>;

class Integer {
 public:
  // Decodes a prefixed integer (RFC 7541 5.1) starting at buf[pos]; `prefixBits` is
  // how many low bits of buf[pos] carry the integer (the rest are representation
  // flags already consumed by the caller). Advances `pos` past the integer's encoding.
  static uint64_t decode(uint8_t const* buf, size_t len, size_t& pos, uint8_t prefixBits);

  // Encodes `value` using `prefixBits` bits in the first byte, OR'd with `firstByteFlags`
  // (the representation-specific high bits, already shifted into position by the caller).
  static std::string encode(uint64_t value, uint8_t prefixBits, uint8_t firstByteFlags);
};

class Huffman {
 public:
  // Decode-only (see file header comment for why we don't encode with Huffman).
  static std::string decode(uint8_t const* buf, size_t len);
};

class StaticTable {
 public:
  // 1-based indices, per RFC 7541 Appendix A
  static Header const& AT(size_t index);
  static size_t SIZE();
  // 1-based index of an exact name+value match, or 0 if none
  static size_t FIND(std::string const& name, std::string const& value);
  // 1-based index of a name-only match, or 0 if none
  static size_t FIND_NAME(std::string const& name);
};

class DynamicTable {
 public:
  explicit DynamicTable(size_t maxSize = _MKN_RAM_HTTP2_HPACK_TABLE_SIZE_) : _max(maxSize) {}
  void add(std::string const& name, std::string const& value);
  void resize(size_t maxSize);
  size_t size() const { return _size; }
  size_t count() const { return _entries.size(); }
  // 0-based, most-recently-added first (RFC 7541 2.3.2 addressing order)
  Header const& at(size_t idx) const { return _entries.at(idx); }

 private:
  std::deque<Header> _entries;
  size_t _size = 0;
  size_t _max;
  void evict();
};

class Decoder {
 public:
  explicit Decoder(size_t maxTableSize = _MKN_RAM_HTTP2_HPACK_TABLE_SIZE_) : _table(maxTableSize) {}
  Headers decode(std::string const& block);

 private:
  DynamicTable _table;
};

class Encoder {
 public:
  // `headers` must list pseudo-headers (":method" etc.) first, per RFC 7540 8.1.2.1.
  static std::string encode(Headers const& headers);
};

}  // namespace hpack
}  // namespace http2
}  // namespace ram
}  // namespace mkn

#endif /* _MKN_RAM_HTTP2_HPACK_HPP_ */
