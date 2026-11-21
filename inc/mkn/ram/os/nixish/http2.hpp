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
#ifndef _MKN_RAM_OS_NIXISH_HTTP2_HPP_
#define _MKN_RAM_OS_NIXISH_HTTP2_HPP_

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "mkn/kul/os.hpp"
#include "mkn/kul/threads.hpp"
#include "mkn/ram/http2/def.hpp"
#include "mkn/ram/http2/frame.hpp"
#include "mkn/ram/http2/hpack.hpp"

namespace mkn {
namespace ram {
namespace http2 {

// A single TLS+ALPN("h2") connection to one host. Supports one client-initiated
// request/response exchange at a time (sequential requests over the same connection
// are fine - each gets the next odd stream id - but there is no concurrent
// multiplexing of in-flight requests from one Client).
class Client {
 protected:
  // the one in-flight request/response exchange
  struct Exchange {
    uint32_t id = 0;
    Response res;
    std::string headerFragment;
    bool endAfterHeaders = false;
    bool done = false;
    int64_t sendWindow = 0;
    int64_t recvConsumed = 0;
  };

  int _sck = -1;
  SSL_CTX* _ctx = nullptr;
  SSL* _ssl = nullptr;
  std::string _host;
  uint16_t _port;
  bool _connected = false;
  uint32_t _nextStreamId = 1;

  hpack::Decoder _decoder;

  // peer-advertised SETTINGS, defaults per RFC 7540 6.5.2 until updated
  uint32_t _peerMaxFrameSize = _MKN_RAM_HTTP2_MAX_FRAME_SIZE_;
  int64_t _peerInitialWindowSize = _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_;

  int64_t _connSendWindow = _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_;
  size_t _connRecvConsumed = 0;

  void sslWrite(std::string const& s);
  std::string sslReadExact(size_t n);
  frame::Header readFrameHeader();
  void handleSettings(frame::Header const& h, std::string const& payload, Exchange& ex);
  void handleWindowUpdate(frame::Header const& h, std::string const& payload, Exchange& ex);
  void handlePing(frame::Header const& h, std::string const& payload);
  void maybeSendConnectionWindowUpdate();
  // reads and handles exactly one frame from the peer on behalf of `ex`
  void readFrame(Exchange& ex);

 public:
  Client(std::string const& host, uint16_t const& port = 443) : _host(host), _port(port) {}
  virtual ~Client() { close(); }

  Client& connect();
  Response request(Request const& req);
  void close();
};

inline Response get(std::string const& host, std::string const& path = "/",
                    uint16_t const& port = 443) {
  Client c(host, port);
  return c.request(Request(host, path, port));
}

inline Response post(std::string const& host, std::string const& path, std::string const& body,
                     uint16_t const& port = 443) {
  Client c(host, port);
  return c.request(Request(host, path, port).withMethod("POST").withBody(body));
}

// TLS+ALPN("h2") server. A single I/O thread owns every connection (non-blocking
// sockets, one poll set) and does all framing, HPACK and flow control; each complete
// request is handed to a fixed-size worker pool for respond(), and the Response is
// handed back to the I/O thread to be written. Thread count is therefore independent
// of connection count, and streams on one connection are served concurrently.
class Server {
 protected:
  struct Connection;
  struct Pending {
    uint64_t connection;
    uint32_t stream;
    Response res;
  };

  uint16_t _port;
  mkn::kul::File const _crt, _key;
  int _lisock = -1;
  int _wake[2] = {-1, -1};  // workers -> I/O thread "responses ready" pipe
  SSL_CTX* _ctx = nullptr;
  std::atomic<bool> _run{false};
  std::thread _io;
  mkn::kul::ThreadPool<> _pool;

  uint64_t _nextConnection = 0;
  std::unordered_map<uint64_t, std::unique_ptr<Connection>> _connections;
  std::vector<char> _readBuffer;

  std::mutex _pendingMutex;
  std::vector<Pending> _pending;

  void operate();
  void acceptAll();
  void drainPending();
  bool service(Connection& c);
  bool readAll(Connection& c);
  bool parse(Connection& c);
  bool onFrame(Connection& c, frame::Header const& fh, std::string& payload);
  bool onHeaderBlock(Connection& c);
  void dispatch(Connection& c, uint32_t const& stream);
  void startResponse(Connection& c, uint32_t const& stream, Response&& res);
  void flushData(Connection& c);
  bool flushOut(Connection& c);
  bool connectionError(Connection& c, frame::Error const& e);
  void closeConnection(uint64_t const& id);
  void post(uint64_t const& connection, uint32_t const& stream, Response&& res);

 public:
  // `port` 0 binds an ephemeral port, see port() after init()
  Server(uint16_t const& port, mkn::kul::File const& crt, mkn::kul::File const& key,
         uint16_t const& workers = 4);
  virtual ~Server();

  Server& init();   // TLS context + listening socket
  Server& start();  // non-blocking, spawns the I/O thread
  void stop();
  uint16_t const& port() const { return _port; }

  // called on a worker thread, once per request; exceptions become a 500
  virtual Response respond(Request const& req);
};

}  // namespace http2
}  // namespace ram
}  // namespace mkn

#endif /* _MKN_RAM_OS_NIXISH_HTTP2_HPP_ */
