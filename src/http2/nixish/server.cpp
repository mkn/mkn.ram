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
#include "mkn/ram/http2.hpp"
#include "mkn/ram/https.hpp"
#include "mkn/ram/tcp.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstring>
#include <map>

namespace {

// RFC 7540 8.1.2.2 - connection-specific header fields are not valid in HTTP/2
bool CONNECTION_SPECIFIC(std::string const& name) {
  return name == "connection" || name == "keep-alive" || name == "proxy-connection" ||
         name == "transfer-encoding" || name == "upgrade";
}

bool SET_NONBLOCKING(int const& fd) {
  int const flags = fcntl(fd, F_GETFL, 0);
  return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

int ALPN_SELECT(SSL* /*ssl*/, unsigned char const** out, unsigned char* outlen,
                unsigned char const* in, unsigned int inlen, void* /*arg*/) {
  static unsigned char const h2[] = {2, 'h', '2'};
  if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen, h2, sizeof(h2), in, inlen) !=
      OPENSSL_NPN_NEGOTIATED)
    return SSL_TLSEXT_ERR_ALERT_FATAL;  // h2 only, no HTTP/1.1 fallback
  return SSL_TLSEXT_ERR_OK;
}

}  // namespace

// All Connection/Stream state is owned and touched by the I/O thread only.
struct mkn::ram::http2::Server::Connection {
  struct Stream {
    hpack::Headers headers;
    std::string body;
    int64_t sendWindow = 0;
    int64_t recvConsumed = 0;
    bool dispatched = false;
    bool responding = false;  // response HEADERS sent, `out` still being sent as DATA
    std::string out;
    size_t outPos = 0;
  };
  enum class State { HANDSHAKE, PREFACE, OPEN };

  uint64_t id;
  int fd;
  SSL* ssl;
  State state = State::HANDSHAKE;
  bool wantWrite = false;  // last SSL call wants the socket writable
  bool closing = false;    // close once `out` is flushed

  std::string in, out;
  size_t outPos = 0;

  hpack::Decoder decoder;
  uint32_t peerMaxFrameSize = _MKN_RAM_HTTP2_MAX_FRAME_SIZE_;
  int64_t peerInitialWindowSize = _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_;
  int64_t sendWindow = _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_;
  size_t recvConsumed = 0;
  uint32_t lastStreamId = 0;

  // a header block in progress (HEADERS without END_HEADERS, then CONTINUATIONs)
  uint32_t blockStream = 0;
  bool blockEndStream = false;
  bool inBlock = false;
  std::string block;

  std::map<uint32_t, Stream> streams;

  Connection(uint64_t const& i, int const& f, SSL* s) : id(i), fd(f), ssl(s) {}
  ~Connection() {
    if (ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
    }
    if (fd >= 0) ::close(fd);
  }
  void queue(std::string const& bytes) { out += bytes; }
  bool pendingOut() const { return outPos < out.size(); }
};

mkn::ram::http2::Server::Server(uint16_t const& port, mkn::kul::File const& crt,
                                mkn::kul::File const& key, uint16_t const& workers)
    : _port(port),
      _crt(crt),
      _key(key),
      _pool(workers, false),
      _readBuffer(_MKN_RAM_HTTP2_READ_BUFFER_) {
  if (workers < 1) KEXCEPT(Exception, "HTTP2 Server cannot have less than one worker thread");
}

mkn::ram::http2::Server::~Server() {
  stop();
  if (_ctx) SSL_CTX_free(_ctx);
}

mkn::ram::http2::Server& mkn::ram::http2::Server::init() {
  _ctx = mkn::ram::https::Server::CONTEXT(_crt, _key);
  SSL_CTX_set_min_proto_version(_ctx, TLS1_2_VERSION);  // RFC 7540 9.2
  // non-blocking writes: keep partial progress, and `out` may reallocate between retries
  SSL_CTX_set_mode(_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  SSL_CTX_set_alpn_select_cb(_ctx, ALPN_SELECT, nullptr);

  struct sockaddr_in addr;
  _lisock = mkn::ram::tcp::SocketServer<>::BIND(_port, addr);
  if (::listen(_lisock, 256) < 0) KEXCEPT(Exception, "HTTP2 Server error on listen");
  socklen_t len = sizeof(addr);
  if (getsockname(_lisock, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
    _port = ntohs(addr.sin_port);

  if (pipe(_wake) < 0 || !SET_NONBLOCKING(_wake[0]) || !SET_NONBLOCKING(_wake[1]))
    KEXCEPT(Exception, "HTTP2 Server error creating wake pipe");
  return *this;
}

mkn::ram::http2::Server& mkn::ram::http2::Server::start() {
  if (!_ctx) init();
  if (_run) return *this;
  _run = true;
  _pool.start();
  _io = std::thread([this]() {
    // a peer closing mid-write must not kill the process: SIGPIPE stays pending on this
    // thread instead, and the write fails with EPIPE
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
    operate();
  });
  return *this;
}

void mkn::ram::http2::Server::stop() {
  bool const started = _io.joinable();
  if (_run) {
    _run = false;
    if (_wake[1] >= 0) (void)!::write(_wake[1], "x", 1);
  }
  if (started) {
    _io.join();
    _pool.stop().join();  // in-flight respond() calls finish; their responses are dropped
  }
  _connections.clear();
  for (auto& fd : _wake)
    if (fd >= 0) ::close(fd), fd = -1;
  if (_lisock >= 0) ::close(_lisock), _lisock = -1;
}

mkn::ram::http2::Response mkn::ram::http2::Server::respond(Request const& /*req*/) {
  Response res;
  res.status(404);
  return res;
}

void mkn::ram::http2::Server::post(uint64_t const& connection, uint32_t const& stream,
                                   Response&& res) {
  {
    std::lock_guard<std::mutex> lock(_pendingMutex);
    _pending.push_back(Pending{connection, stream, std::move(res)});
  }
  (void)!::write(_wake[1], "x", 1);  // EAGAIN means a wake-up is already pending
}

void mkn::ram::http2::Server::operate() {
  std::vector<struct pollfd> pfds;
  std::vector<uint64_t> ids;
  while (_run) {
    pfds.clear();
    ids.clear();
    pfds.push_back({_lisock, POLLIN, 0});
    pfds.push_back({_wake[0], POLLIN, 0});
    for (auto const& p : _connections) {
      short events = POLLIN;
      if (p.second->wantWrite || p.second->pendingOut()) events |= POLLOUT;
      pfds.push_back({p.second->fd, events, 0});
      ids.push_back(p.first);
    }
    int const ret = ::poll(pfds.data(), pfds.size(), 100);
    if (!_run) break;
    if (ret < 0) {
      if (errno != EINTR) KLOG(ERR) << "HTTP2 Server error on poll: " << strerror(errno);
      continue;
    }
    if (ret == 0) continue;
    if (pfds[1].revents & POLLIN) drainPending();
    if (pfds[0].revents & POLLIN) acceptAll();
    for (size_t i = 0; i < ids.size(); ++i) {
      auto const revents = pfds[i + 2].revents;
      if (!revents) continue;
      auto it = _connections.find(ids[i]);
      if (it == _connections.end()) continue;
      if ((revents & (POLLERR | POLLNVAL)) || !service(*it->second)) closeConnection(ids[i]);
    }
  }
}

void mkn::ram::http2::Server::acceptAll() {
  while (true) {
    int const fd = ::accept(_lisock, nullptr, nullptr);
    if (fd < 0) {
      if (errno == EINTR) continue;
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        KLOG(ERR) << "HTTP2 Server error on accept: " << strerror(errno);
      return;
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#if defined(SO_NOSIGPIPE)
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    SSL* ssl = SET_NONBLOCKING(fd) ? SSL_new(_ctx) : nullptr;
    if (!ssl) {
      KLOG(ERR) << "HTTP2 Server failed to set up accepted connection";
      ::close(fd);
      continue;
    }
    SSL_set_fd(ssl, fd);
    SSL_set_accept_state(ssl);
    auto const id = ++_nextConnection;
    _connections.emplace(id, std::make_unique<Connection>(id, fd, ssl));
  }
}

void mkn::ram::http2::Server::closeConnection(uint64_t const& id) { _connections.erase(id); }

bool mkn::ram::http2::Server::service(Connection& c) {
  if (c.state == Connection::State::HANDSHAKE) {
    int const r = SSL_do_handshake(c.ssl);
    if (r != 1) {
      int const e = SSL_get_error(c.ssl, r);
      c.wantWrite = e == SSL_ERROR_WANT_WRITE;
      return e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE;
    }
    c.wantWrite = false;
    unsigned char const* proto = nullptr;
    unsigned int protoLen = 0;
    SSL_get0_alpn_selected(c.ssl, &proto, &protoLen);
    if (!proto || std::string(reinterpret_cast<char const*>(proto), protoLen) != "h2") return false;
    c.state = Connection::State::PREFACE;
    // server connection preface, RFC 7540 3.5
    std::vector<frame::settings::Entry> const settings{
        {frame::settings::MAX_CONCURRENT_STREAMS, _MKN_RAM_HTTP2_MAX_CONCURRENT_STREAMS_}};
    c.queue(frame::BUILD(frame::Type::SETTINGS, 0, 0, frame::settings::BUILD(settings)));
  }
  if (!readAll(c)) return false;
  if (!flushOut(c)) return false;
  return !(c.closing && !c.pendingOut());
}

bool mkn::ram::http2::Server::readAll(Connection& c) {
  while (!c.closing) {
    int const n = SSL_read(c.ssl, _readBuffer.data(), static_cast<int>(_readBuffer.size()));
    if (n > 0) {
      c.wantWrite = false;
      c.in.append(_readBuffer.data(), static_cast<size_t>(n));
      if (!parse(c)) return false;
      continue;
    }
    int const e = SSL_get_error(c.ssl, n);
    c.wantWrite = e == SSL_ERROR_WANT_WRITE;
    return e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE;  // else EOF or error
  }
  return true;
}

bool mkn::ram::http2::Server::parse(Connection& c) {
  size_t pos = 0;
  if (c.state == Connection::State::PREFACE) {
    auto const& preface = frame::CONNECTION_PREFACE();
    size_t const n = std::min(preface.size(), c.in.size());
    if (c.in.compare(0, n, preface, 0, n) != 0) return false;
    if (n < preface.size()) return true;
    pos = preface.size();
    c.state = Connection::State::OPEN;
  }
  while (!c.closing && c.in.size() - pos >= frame::HEADER_LEN) {
    auto const fh = frame::Header::PARSE(reinterpret_cast<uint8_t const*>(c.in.data() + pos));
    if (fh.length > _MKN_RAM_HTTP2_MAX_FRAME_SIZE_) {
      connectionError(c, frame::Error::FRAME_SIZE);
      break;
    }
    if (c.in.size() - pos - frame::HEADER_LEN < fh.length) break;
    std::string payload = c.in.substr(pos + frame::HEADER_LEN, fh.length);
    pos += frame::HEADER_LEN + fh.length;
    onFrame(c, fh, payload);
  }
  c.in.erase(0, pos);
  return true;
}

bool mkn::ram::http2::Server::connectionError(Connection& c, frame::Error const& e) {
  c.queue(frame::GoAway::BUILD(c.lastStreamId, e));
  c.closing = true;
  return false;
}

bool mkn::ram::http2::Server::onFrame(Connection& c, frame::Header const& fh,
                                      std::string& payload) {
  // RFC 7540 6.10 - nothing may interleave with a header block in progress
  if (c.inBlock && (fh.type != frame::Type::CONTINUATION || fh.stream_id != c.blockStream))
    return connectionError(c, frame::Error::PROTOCOL);

  switch (fh.type) {
    case frame::Type::SETTINGS: {
      if (fh.stream_id != 0) return connectionError(c, frame::Error::PROTOCOL);
      if (fh.flags & frame::flag::ACK) return true;
      if (payload.size() % 6) return connectionError(c, frame::Error::FRAME_SIZE);
      for (auto const& e : frame::settings::PARSE(payload)) {
        if (e.first == frame::settings::MAX_FRAME_SIZE) {
          if (e.second < _MKN_RAM_HTTP2_MAX_FRAME_SIZE_ || e.second > 0xFFFFFF)
            return connectionError(c, frame::Error::PROTOCOL);
          c.peerMaxFrameSize = e.second;
        } else if (e.first == frame::settings::INITIAL_WINDOW_SIZE) {
          if (e.second > frame::MAX_WINDOW_SIZE)
            return connectionError(c, frame::Error::FLOW_CONTROL);
          int64_t const delta = static_cast<int64_t>(e.second) - c.peerInitialWindowSize;
          for (auto& s : c.streams) s.second.sendWindow += delta;
          c.peerInitialWindowSize = e.second;
        }
      }
      c.queue(frame::BUILD(frame::Type::SETTINGS, frame::flag::ACK, 0));
      flushData(c);
      return true;
    }
    case frame::Type::PING: {
      if (fh.stream_id != 0) return connectionError(c, frame::Error::PROTOCOL);
      if (payload.size() != 8) return connectionError(c, frame::Error::FRAME_SIZE);
      if (!(fh.flags & frame::flag::ACK))
        c.queue(frame::BUILD(frame::Type::PING, frame::flag::ACK, 0, payload));
      return true;
    }
    case frame::Type::WINDOW_UPDATE: {
      if (payload.size() != 4) return connectionError(c, frame::Error::FRAME_SIZE);
      uint32_t const inc = frame::WINDOW_UPDATE_INCREMENT(payload);
      if (fh.stream_id == 0) {
        if (inc == 0) return connectionError(c, frame::Error::PROTOCOL);
        c.sendWindow += inc;
        if (c.sendWindow > frame::MAX_WINDOW_SIZE)
          return connectionError(c, frame::Error::FLOW_CONTROL);
      } else {
        auto it = c.streams.find(fh.stream_id);
        if (it == c.streams.end()) return true;  // closed stream, RFC 7540 6.9
        if (inc == 0) {
          c.queue(frame::RST_STREAM(fh.stream_id, frame::Error::PROTOCOL));
          c.streams.erase(it);
          return true;
        }
        it->second.sendWindow += inc;
      }
      flushData(c);
      return true;
    }
    case frame::Type::HEADERS: {
      uint32_t const sid = fh.stream_id;
      if (sid == 0 || !(sid & 1)) return connectionError(c, frame::Error::PROTOCOL);
      if (!frame::HEADER_FRAGMENT(fh, payload)) return connectionError(c, frame::Error::PROTOCOL);
      if (sid > c.lastStreamId) {
        c.lastStreamId = sid;
        // a refused stream gets no entry, but its block is still decoded to keep HPACK in sync
        if (c.streams.size() >= _MKN_RAM_HTTP2_MAX_CONCURRENT_STREAMS_)
          c.queue(frame::RST_STREAM(sid, frame::Error::REFUSED_STREAM));
        else
          c.streams[sid].sendWindow = c.peerInitialWindowSize;
      } else {
        // trailers on a stream still receiving its body, else a closed stream
        auto it = c.streams.find(sid);
        if (it == c.streams.end() || it->second.dispatched)
          return connectionError(c, frame::Error::STREAM_CLOSED);
        if (!(fh.flags & frame::flag::END_STREAM))
          return connectionError(c, frame::Error::PROTOCOL);
      }
      c.blockStream = sid;
      c.block = payload;
      c.blockEndStream = fh.flags & frame::flag::END_STREAM;
      if (fh.flags & frame::flag::END_HEADERS) return onHeaderBlock(c);
      c.inBlock = true;
      return true;
    }
    case frame::Type::CONTINUATION: {
      if (!c.inBlock) return connectionError(c, frame::Error::PROTOCOL);
      c.block += payload;
      if (!(fh.flags & frame::flag::END_HEADERS)) return true;
      c.inBlock = false;
      return onHeaderBlock(c);
    }
    case frame::Type::DATA: {
      if (fh.stream_id == 0) return connectionError(c, frame::Error::PROTOCOL);
      c.recvConsumed += fh.length;
      if (c.recvConsumed > _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_ / 2) {
        c.queue(frame::BUILD(frame::Type::WINDOW_UPDATE, 0, 0,
                             frame::WINDOW_UPDATE_PAYLOAD(static_cast<uint32_t>(c.recvConsumed))));
        c.recvConsumed = 0;
      }
      auto it = c.streams.find(fh.stream_id);
      if (it == c.streams.end() || it->second.dispatched) {
        if (fh.stream_id > c.lastStreamId) return connectionError(c, frame::Error::PROTOCOL);
        c.queue(frame::RST_STREAM(fh.stream_id, frame::Error::STREAM_CLOSED));
        return true;
      }
      if (!frame::UNPAD(fh, payload)) return connectionError(c, frame::Error::PROTOCOL);
      auto& s = it->second;
      s.body += payload;
      s.recvConsumed += fh.length;
      if (fh.flags & frame::flag::END_STREAM)
        dispatch(c, fh.stream_id);
      else if (s.recvConsumed > _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_ / 2) {
        c.queue(frame::BUILD(frame::Type::WINDOW_UPDATE, 0, fh.stream_id,
                             frame::WINDOW_UPDATE_PAYLOAD(static_cast<uint32_t>(s.recvConsumed))));
        s.recvConsumed = 0;
      }
      return true;
    }
    case frame::Type::RST_STREAM: {
      if (fh.stream_id == 0) return connectionError(c, frame::Error::PROTOCOL);
      c.streams.erase(fh.stream_id);  // an in-flight response for it is dropped in drainPending
      return true;
    }
    case frame::Type::GOAWAY:
      c.closing = true;
      return true;
    case frame::Type::PUSH_PROMISE:  // clients cannot push, RFC 7540 8.2
      return connectionError(c, frame::Error::PROTOCOL);
    default:
      return true;  // PRIORITY / unknown - ignore
  }
}

bool mkn::ram::http2::Server::onHeaderBlock(Connection& c) {
  hpack::Headers headers;
  try {
    headers = c.decoder.decode(c.block);
  } catch (hpack::Exception const&) {
    return connectionError(c, frame::Error::COMPRESSION);
  }
  c.block.clear();
  auto it = c.streams.find(c.blockStream);
  if (it == c.streams.end()) return true;  // refused stream, decoded only for HPACK state
  for (auto& h : headers) it->second.headers.push_back(std::move(h));
  if (c.blockEndStream) dispatch(c, c.blockStream);
  return true;
}

void mkn::ram::http2::Server::dispatch(Connection& c, uint32_t const& sid) {
  auto& s = c.streams.at(sid);
  s.dispatched = true;

  std::string method, path, authority, scheme;
  Headers headers;
  for (auto const& h : s.headers) {
    if (h.first == ":method")
      method = h.second;
    else if (h.first == ":path")
      path = h.second;
    else if (h.first == ":authority")
      authority = h.second;
    else if (h.first == ":scheme")
      scheme = h.second;
    else if (!h.first.empty() && h.first[0] != ':')
      headers[h.first] = h.second;
  }
  if (authority.empty() && headers.count("host")) authority = headers["host"];
  if (method.empty() || path.empty()) {  // RFC 7540 8.1.2.6 - malformed request
    c.queue(frame::RST_STREAM(sid, frame::Error::PROTOCOL));
    c.streams.erase(sid);
    return;
  }

  Request req(authority, path, _port);
  req.withMethod(method).withHeaders(headers).withBody(s.body);
  if (!scheme.empty()) req.withScheme(scheme);
  s.headers.clear();
  s.body.clear();

  auto const cid = c.id;
  _pool.async([this, cid, sid, req]() {
    Response res;
    try {
      res = respond(req);
    } catch (std::exception const& e) {
      KERR << "HTTP2 Server respond threw: " << e.what();
      res = Response();
      res.status(500);
    } catch (...) {
      KERR << "HTTP2 Server respond threw";
      res = Response();
      res.status(500);
    }
    post(cid, sid, std::move(res));
  });
}

void mkn::ram::http2::Server::drainPending() {
  char buf[256];
  while (::read(_wake[0], buf, sizeof(buf)) > 0) {
  }
  std::vector<Pending> pending;
  {
    std::lock_guard<std::mutex> lock(_pendingMutex);
    pending.swap(_pending);
  }
  std::vector<uint64_t> touched;
  for (auto& p : pending) {
    auto it = _connections.find(p.connection);
    if (it == _connections.end()) continue;  // connection gone while the worker ran
    auto& c = *it->second;
    if (!c.streams.count(p.stream)) continue;  // stream reset while the worker ran
    startResponse(c, p.stream, std::move(p.res));
    touched.push_back(p.connection);
  }
  for (auto const& id : touched) {
    auto it = _connections.find(id);
    if (it != _connections.end() && !flushOut(*it->second)) closeConnection(id);
  }
}

void mkn::ram::http2::Server::startResponse(Connection& c, uint32_t const& sid, Response&& res) {
  hpack::Headers headers{{":status", std::to_string(res.status() ? res.status() : 200)}};
  for (auto const& h : res.headers()) {
    std::string name = h.first;  // RFC 7540 8.1.2 - field names must be lowercase
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (!CONNECTION_SPECIFIC(name)) headers.emplace_back(std::move(name), h.second);
  }
  auto& s = c.streams.at(sid);
  bool const empty = res.body().empty();
  c.queue(frame::HEADER_BLOCK(sid, hpack::Encoder::encode(headers), empty, c.peerMaxFrameSize));
  if (empty) {
    c.streams.erase(sid);
    return;
  }
  s.out = res.body();
  s.outPos = 0;
  s.responding = true;
  flushData(c);
}

// Sends as much pending response body as the connection and stream send windows
// allow, round-robin across streams so one large response can't starve the others.
void mkn::ram::http2::Server::flushData(Connection& c) {
  bool progressed = true;
  while (progressed && c.sendWindow > 0) {
    progressed = false;
    for (auto it = c.streams.begin(); it != c.streams.end() && c.sendWindow > 0;) {
      auto& s = it->second;
      if (!s.responding || s.sendWindow <= 0) {
        ++it;
        continue;
      }
      size_t const n =
          std::min({s.out.size() - s.outPos, static_cast<size_t>(c.peerMaxFrameSize),
                    static_cast<size_t>(s.sendWindow), static_cast<size_t>(c.sendWindow)});
      bool const last = s.outPos + n >= s.out.size();
      c.queue(frame::BUILD(frame::Type::DATA, last ? frame::flag::END_STREAM : 0, it->first,
                           s.out.substr(s.outPos, n)));
      s.outPos += n;
      s.sendWindow -= static_cast<int64_t>(n);
      c.sendWindow -= static_cast<int64_t>(n);
      progressed = true;
      if (last)
        it = c.streams.erase(it);
      else
        ++it;
    }
  }
}

bool mkn::ram::http2::Server::flushOut(Connection& c) {
  while (c.pendingOut()) {
    size_t const left = std::min(c.out.size() - c.outPos, static_cast<size_t>(INT_MAX));
    int const n = SSL_write(c.ssl, c.out.data() + c.outPos, static_cast<int>(left));
    if (n > 0) {
      c.outPos += static_cast<size_t>(n);
      continue;
    }
    int const e = SSL_get_error(c.ssl, n);
    if (e == SSL_ERROR_WANT_WRITE || e == SSL_ERROR_WANT_READ) {
      c.wantWrite = e == SSL_ERROR_WANT_WRITE;
      return true;
    }
    return false;
  }
  c.out.clear();
  c.outPos = 0;
  return true;
}
