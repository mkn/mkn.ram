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
#include "mkn/kul/asio/fcgi.hpp"

#include "mkn/kul/signal.hpp"

int main(int argc, char* argv[]) {
  mkn::kul::Signal sig;
  try {
    mkn::ram::asio::fcgi::Server s(5863, 4, 8);

    sig.intr([&](int16_t) {
      KERR << "Interrupted";
      s.interrupt();
      s.join();
      exit(2);
    });
    s.join();
  } catch (mkn::kul::Exception const& e) {
    KERR << e.stack();
  } catch (std::exception const& e) {
    KERR << e.what();
  } catch (...) {
    KERR << "UNKNOWN EXCEPTION CAUGHT";
  }
  return 0;
}

// #include <iostream>
// #include "fcgio.h"

// using namespace std;

// int main(void) {
//     // Backup the stdio streambufs
//     streambuf * cin_streambuf  = cin.rdbuf();
//     streambuf * cout_streambuf = cout.rdbuf();
//     streambuf * cerr_streambuf = cerr.rdbuf();

//     FCGX_Request request;

//     FCGX_Init();
//     FCGX_InitRequest(&request, 0, 0);

//     while (FCGX_Accept_r(&request) == 0) {
//         fcgi_streambuf cin_fcgi_streambuf(request.in);
//         fcgi_streambuf cout_fcgi_streambuf(request.out);
//         fcgi_streambuf cerr_fcgi_streambuf(request.err);

//         cin.rdbuf(&cin_fcgi_streambuf);
//         cout.rdbuf(&cout_fcgi_streambuf);
//         cerr.rdbuf(&cerr_fcgi_streambuf);

//         cout << "Content-type: text/html\r\n"
//              << "\r\n"
//              << "<html>\n"
//              << "  <head>\n"
//              << "    <title>Hello, World!</title>\n"
//              << "  </head>\n"
//              << "  <body>\n"
//              << "    <h1>Hello, World!</h1>\n"
//              << "  </body>\n"
//              << "</html>\n";

//         // Note: the fcgi_streambuf destructor will auto flush
//     }

//     // restore stdio streambufs
//     cin.rdbuf(cin_streambuf);
//     cout.rdbuf(cout_streambuf);
//     cerr.rdbuf(cerr_streambuf);

//     return 0;
// }
