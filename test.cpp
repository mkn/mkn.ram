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
#define __KUL_RAM_NOMAIN__
#include "usage.cpp"

int main(int argc, char* argv[]) {
    kul::Signal s;

    uint8_t _threads = 4;

    auto getter = [](){
        kul::ram::Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();
    };
    auto except = [](const kul::Exception& e){
        KLOG(ERR) << e.stack();
    };

    kul::ram::TestMultiHTTPServer serv;
    try{

        // kul::ChroncurrentThreadPool<> ctp(_threads, 1);
        // for(size_t i = 0; i < 10000; i++) ctp.async(getter, except);
        for(size_t i = 0; i < 1000; i++) {
            KLOG(INF) << "SENDING: " << i;
            kul::ram::Get("localhost", "index.html", _KUL_HTTP_TEST_PORT_).send();
            if(serv.exception()) std::rethrow_exception(serv.exception());
            KLOG(INF) << i;
        }
        // ctp.finish(1000000000);

    }
    catch(const kul::Exception& e){ KERR << e.stack(); }
    catch(const std::exception& e){ KERR << e.what(); }
    catch(...)                    { KERR << "UNKNOWN EXCEPTION CAUGHT"; }
    kul::this_thread::sleep(100);
    serv.stop();
    serv.join();

    return 0;
}