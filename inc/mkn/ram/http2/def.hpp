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
#ifndef _MKN_RAM_HTTP2_DEF_HPP_
#define _MKN_RAM_HTTP2_DEF_HPP_

// RFC 7540 6.9.2 - the default initial flow-control window, both connection
// and per-stream, before any SETTINGS_INITIAL_WINDOW_SIZE/WINDOW_UPDATE is applied
#ifndef _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_
#define _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_ 65535
#endif /* _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_ */

// RFC 7540 4.2 - the default max frame payload size we assume for the peer
// until its SETTINGS_MAX_FRAME_SIZE says otherwise
#ifndef _MKN_RAM_HTTP2_MAX_FRAME_SIZE_
#define _MKN_RAM_HTTP2_MAX_FRAME_SIZE_ 16384
#endif /* _MKN_RAM_HTTP2_MAX_FRAME_SIZE_ */

// Buffer size used for raw SSL_read chunks while draining frames off the wire
#ifndef _MKN_RAM_HTTP2_READ_BUFFER_
#define _MKN_RAM_HTTP2_READ_BUFFER_ 65536
#endif /* _MKN_RAM_HTTP2_READ_BUFFER_ */

// RFC 7541 4.2 - default HPACK dynamic table size cap (both encode and decode sides)
#ifndef _MKN_RAM_HTTP2_HPACK_TABLE_SIZE_
#define _MKN_RAM_HTTP2_HPACK_TABLE_SIZE_ 4096
#endif /* _MKN_RAM_HTTP2_HPACK_TABLE_SIZE_ */

#endif /* _MKN_RAM_HTTP2_DEF_HPP_ */
