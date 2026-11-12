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
#ifndef MKN_RAM_FCGI_HPP
#define MKN_RAM_FCGI_HPP

// FastCGI responder servers, for running behind a web server such as nginx or apache.
// Handlers take and return the http types, so one handler can serve http, https and fcgi.
//
//   Server       one thread, requests are handled in turn on the connection loop
//   AsyncServer  the connection loop hands requests to a worker pool, connections are
//                multiplexed
//
// The record codec and Request are portable, the servers are POSIX only.

#include "mkn/ram/http.hpp"  // first, windows.h before mkn/kul/threads.hpp on Windows
#include "mkn/kul/except.hpp"
#include "mkn/kul/threads.hpp"
#include "mkn/ram/fcgi/def.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mkn::ram::fcgi {

class Exception : public mkn::kul::Exception {
 public:
  Exception(char const* f, std::uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

using Params = std::unordered_map<std::string, std::string>;

namespace record {

struct Record {
  std::uint8_t type = 0;
  std::uint16_t id = 0;
  std::string content;
};

// Appends records of `type` carrying `content`, split at MAX_CONTENT and padded to 8 bytes.
// Empty content appends one empty record, which ends a stream.
void append(std::string& out, std::uint8_t type, std::uint16_t id, std::string_view content);
void begin(std::string& out, std::uint16_t id, std::uint16_t role, std::uint8_t flags);
void end(std::string& out, std::uint16_t id, std::uint8_t protocolStatus,
         std::uint32_t appStatus = 0);

// name-value pairs, as in PARAMS, GET_VALUES and GET_VALUES_RESULT
std::string encode(Params const& params);
Params decode(std::string_view content) KTHROW(Exception);

// CGI response: "Status:" line, headers less hop-by-hop ones, cookies, blank line, body
std::string cgi(http::Response const& response);
// STDOUT records of the CGI response, the end of STDOUT, and END_REQUEST
std::string respond(std::uint16_t id, http::Response const& response);

// Splits a byte stream into records, keeping partial input until it completes
class Parser {
 public:
  void feed(char const* data, std::size_t size) { _buf.append(data, size); }
  bool next(Record& record) KTHROW(Exception);

 private:
  std::string _buf;
  std::size_t _pos = 0;
};

}  // namespace record

// An http request decoded from CGI params and STDIN:
//   method       REQUEST_METHOD
//   path         REQUEST_URI up to '?', else SCRIPT_NAME + PATH_INFO, not percent-decoded
//   attributes   QUERY_STRING pairs, not percent-decoded
//   host         HTTP_HOST, else SERVER_NAME
//   ip, port     REMOTE_ADDR, REMOTE_PORT
//   headers      HTTP_* as e.g. Accept-Encoding, plus Content-Type and Content-Length
//   cookies      HTTP_COOKIE
// All params are kept, see params().
class MKN_KUL_PUBLISH Request : public http::A1_1Request {
 public:
  Request(Params params, std::string body);
  std::string method() const override { return _method; }
  std::string toString() const override;
  Params const& params() const { return _params; }
  std::string param(std::string const& key) const {
    auto const it = _params.find(key);
    return it == _params.end() ? "" : it->second;
  }

 private:
  std::string _method, _uri;
  Params _params;
};

#if !defined(_WIN32)  // servers are POSIX only

class MKN_KUL_PUBLISH AServer {
 public:
  using Responder = std::function<http::Response(http::A1_1Request const&)>;

  // FastCGI has no authentication, bind to an address only the web server can reach
  AServer(std::uint16_t port, std::string addr = "127.0.0.1");
  virtual ~AServer();

  AServer(AServer const&) = delete;
  AServer& operator=(AServer const&) = delete;

  AServer& withResponse(Responder const& func) {
    _func = func;
    return *this;
  }
  virtual http::Response respond(http::A1_1Request const& req) {
    if (_func) return _func(req);
    KEXCEPTION("mkn::ram::fcgi::AServer::respond - no response defined");
  }

  // binds and listens, port 0 binds an ephemeral port, see port()
  AServer& init() KTHROW(Exception);
  // init()s if needed and runs the connection loop on its own thread
  virtual AServer& start() KTHROW(Exception);
  // stops the loop, open connections are closed, safe from any thread
  virtual AServer& stop();
  // waits for the loop to exit, after stop() or a fatal error
  virtual AServer& join();

  std::uint16_t port() const;
  // a fatal error that ended the loop, if any
  std::exception_ptr exception() const;

 protected:
  // FCGI_MPXS_CONNS, whether a connection may carry concurrent requests
  virtual bool multiplexed() const = 0;
  // called on the loop thread for each complete request, the response goes to complete()
  virtual void dispatch(std::uint64_t conn, std::uint16_t id, std::shared_ptr<Request> req) = 0;
  // respond(), an exception from it is logged and becomes a 500
  http::Response respondSafely(http::A1_1Request const& req) noexcept;
  // hands a response to the loop, safe from any thread, dropped if the connection closed
  void complete(std::uint64_t conn, std::uint16_t id, http::Response const& res);

 private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  Responder _func;

  void loop();
};

// Handles one request at a time on the connection loop thread, connections are not
// multiplexed but several may be open, e.g. kept alive by the web server.
class MKN_KUL_PUBLISH Server : public AServer {
 public:
  Server(std::uint16_t port, std::string addr = "127.0.0.1") : AServer(port, std::move(addr)) {}
  ~Server() override { stop().join(); }

 protected:
  bool multiplexed() const override { return false; }
  void dispatch(std::uint64_t conn, std::uint16_t id, std::shared_ptr<Request> req) override {
    complete(conn, id, respondSafely(*req));
  }
};

// The connection loop hands requests to `workers` threads, requests on one connection
// may be multiplexed.
class MKN_KUL_PUBLISH AsyncServer : public AServer {
 public:
  AsyncServer(std::uint16_t port, std::size_t workers = 1, std::string addr = "127.0.0.1")
      : AServer(port, std::move(addr)), _pool(workers ? workers : 1, false) {}
  ~AsyncServer() override { stop().join(); }

  AsyncServer& start() KTHROW(Exception) override {
    _pool.start();
    AServer::start();
    return *this;
  }
  AsyncServer& stop() override {
    AServer::stop();
    _pool.stop();
    return *this;
  }
  AsyncServer& join() override {
    AServer::join();
    _pool.join();
    return *this;
  }

 protected:
  bool multiplexed() const override { return true; }
  void dispatch(std::uint64_t conn, std::uint16_t id, std::shared_ptr<Request> req) override {
    _pool.async(
        [this, conn, id, req = std::move(req)]() { complete(conn, id, respondSafely(*req)); });
  }

 private:
  mkn::kul::ThreadPool<> _pool;
};

#endif  // !_WIN32

}  // namespace mkn::ram::fcgi

#endif /* MKN_RAM_FCGI_HPP */
