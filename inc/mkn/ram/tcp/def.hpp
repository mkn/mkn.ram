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
#ifndef _MKN_RAM_TCP_DEF_HPP_
#define _MKN_RAM_TCP_DEF_HPP_

#ifndef _MKN_RAM_TCP_SESSION_TTL_
#define _MKN_RAM_TCP_SESSION_TTL_ 600  // seconds
#endif                                 /* _MKN_RAM_TCP_SESSION_TTL_ */

#ifndef _MKN_RAM_TCP_SESSION_CHECK_
#define _MKN_RAM_TCP_SESSION_CHECK_ 10000  // milliseconds to sleep between checks
#endif                                     /* _MKN_RAM_TCP_SESSION_CHECK_ */

#ifndef _MKN_RAM_TCP_READ_BUFFER_
#define _MKN_RAM_TCP_READ_BUFFER_ 963210
#endif /* _MKN_RAM_TCP_READ_BUFFER_ */

#ifndef _MKN_RAM_TCP_MAX_CLIENT_
#define _MKN_RAM_TCP_MAX_CLIENT_ 4096
#endif /* _MKN_RAM_TCP_MAX_CLIENT_ */

#ifndef _MKN_RAM_TCP_REQUEST_BUFFER_
#define _MKN_RAM_TCP_REQUEST_BUFFER_ 963210
#endif /* _MKN_RAM_TCP_REQUEST_BUFFER_ */

#ifdef _WIN32
#define bzero ZeroMemory
#endif

#endif /* _MKN_RAM_TCP_DEF_HPP_ */
