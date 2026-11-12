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
#ifndef MKN_RAM_FCGI_DEF_HPP
#define MKN_RAM_FCGI_DEF_HPP

#include <cstdint>

// connections an fcgi server holds open at once, further ones are closed on accept
#ifndef MKN_RAM_FCGI_MAX_CONNS
#define MKN_RAM_FCGI_MAX_CONNS 1024
#endif /* MKN_RAM_FCGI_MAX_CONNS */

// bytes read from a connection per read call
#ifndef MKN_RAM_FCGI_READ_BUFFER
#define MKN_RAM_FCGI_READ_BUFFER 65536
#endif /* MKN_RAM_FCGI_READ_BUFFER */

// FastCGI 1.0, https://fastcgi-archives.github.io/FastCGI_Specification.html
namespace mkn::ram::fcgi {

constexpr std::uint8_t VERSION_1 = 1;
constexpr std::size_t HEADER_LEN = 8;
constexpr std::size_t MAX_CONTENT = 65535;
constexpr std::uint16_t NULL_REQUEST_ID = 0;

namespace type {
constexpr std::uint8_t BEGIN_REQUEST = 1;
constexpr std::uint8_t ABORT_REQUEST = 2;
constexpr std::uint8_t END_REQUEST = 3;
constexpr std::uint8_t PARAMS = 4;
constexpr std::uint8_t STDIN = 5;
constexpr std::uint8_t STDOUT = 6;
constexpr std::uint8_t STDERR = 7;
constexpr std::uint8_t DATA = 8;
constexpr std::uint8_t GET_VALUES = 9;
constexpr std::uint8_t GET_VALUES_RESULT = 10;
constexpr std::uint8_t UNKNOWN_TYPE = 11;
}  // namespace type

// BEGIN_REQUEST flags
constexpr std::uint8_t KEEP_CONN = 1;

namespace role {
constexpr std::uint16_t RESPONDER = 1;
constexpr std::uint16_t AUTHORIZER = 2;
constexpr std::uint16_t FILTER = 3;
}  // namespace role

// END_REQUEST protocolStatus
namespace status {
constexpr std::uint8_t REQUEST_COMPLETE = 0;
constexpr std::uint8_t CANT_MPX_CONN = 1;
constexpr std::uint8_t OVERLOADED = 2;
constexpr std::uint8_t UNKNOWN_ROLE = 3;
}  // namespace status

}  // namespace mkn::ram::fcgi

#endif /* MKN_RAM_FCGI_DEF_HPP */
