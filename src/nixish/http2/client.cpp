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
#ifdef _MKN_RAM_INCLUDE_HTTP2_
#include "mkn/ram/http2.hpp"

#include <unistd.h>

#include <algorithm>

#include "mkn/ram/tcp.hpp"

void mkn::ram::http2::Client::sslWrite(std::string const& s) {
  size_t sent = 0;
  while (sent < s.size()) {
    int r = SSL_write(_ssl, s.data() + sent, static_cast<int>(s.size() - sent));
    if (r <= 0) KEXCEPT(Exception, "HTTP2: SSL_write failed, host: " + _host);
    sent += static_cast<size_t>(r);
  }
}

std::string mkn::ram::http2::Client::sslReadExact(size_t n) {
  std::string out;
  out.resize(n);
  size_t got = 0;
  while (got < n) {
    int r = SSL_read(_ssl, &out[got], static_cast<int>(n - got));
    if (r <= 0) KEXCEPT(Exception, "HTTP2: SSL_read failed/connection closed, host: " + _host);
    got += static_cast<size_t>(r);
  }
  return out;
}

mkn::ram::http2::frame::Header mkn::ram::http2::Client::readFrameHeader() {
  std::string h = sslReadExact(frame::HEADER_LEN);
  return frame::Header::PARSE(reinterpret_cast<uint8_t const*>(h.data()));
}

void mkn::ram::http2::Client::handleSettings(frame::Header const& fh, std::string const& payload) {
  if (fh.flags & frame::flag::ACK) return;  // peer acknowledging our SETTINGS
  for (auto const& e : frame::settings::PARSE(payload)) {
    if (e.first == frame::settings::MAX_FRAME_SIZE)
      _peerMaxFrameSize = e.second;
    else if (e.first == frame::settings::INITIAL_WINDOW_SIZE)
      _peerInitialWindowSize = e.second;
  }
  sslWrite(frame::BUILD(frame::Type::SETTINGS, frame::flag::ACK, 0));
}

void mkn::ram::http2::Client::handlePing(frame::Header const& fh, std::string const& payload) {
  if (fh.flags & frame::flag::ACK) return;
  sslWrite(frame::BUILD(frame::Type::PING, frame::flag::ACK, 0, payload));
}

void mkn::ram::http2::Client::handleWindowUpdate(frame::Header const& fh,
                                                 std::string const& payload) {
  uint32_t const inc = frame::WINDOW_UPDATE_INCREMENT(payload);
  if (fh.stream_id == 0) _connSendWindow += inc;
  // per-stream send-window tracking is intentionally not modelled: request bodies
  // are the exception for this client, not the norm - see plan/README for follow-up.
}

void mkn::ram::http2::Client::maybeSendConnectionWindowUpdate() {
  if (_connRecvConsumed > _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_ / 2) {
    sslWrite(frame::BUILD(frame::Type::WINDOW_UPDATE, 0, 0,
                          frame::WINDOW_UPDATE_PAYLOAD(static_cast<uint32_t>(_connRecvConsumed))));
    _connRecvConsumed = 0;
  }
}

mkn::ram::http2::Client& mkn::ram::http2::Client::connect() {
  if (_connected) return *this;

  if (!mkn::ram::tcp::Socket<char>::SOCKET(_sck, PF_INET, SOCK_STREAM, 0))
    KEXCEPT(Exception, "HTTP2: error opening socket");
  if (!mkn::ram::tcp::Socket<char>::CONNECT(_sck, _host, _port))
    KEXCEPT(Exception, "HTTP2: failed to connect to host: " + _host);

  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  _ctx = SSL_CTX_new(TLS_client_method());
  if (!_ctx) KEXCEPT(Exception, "HTTP2: SSL_CTX_new failed");

  unsigned char const alpn[] = {2, 'h', '2'};
  if (SSL_CTX_set_alpn_protos(_ctx, alpn, sizeof(alpn)) != 0)
    KEXCEPT(Exception, "HTTP2: SSL_CTX_set_alpn_protos failed");

  _ssl = SSL_new(_ctx);
  if (!_ssl) KEXCEPT(Exception, "HTTP2: SSL_new failed");
  SSL_set_tlsext_host_name(_ssl, _host.c_str());
  SSL_set_fd(_ssl, _sck);
  if (SSL_connect(_ssl) != 1) KEXCEPT(Exception, "HTTP2: TLS handshake failed with host: " + _host);

  unsigned char const* proto = nullptr;
  unsigned int protoLen = 0;
  SSL_get0_alpn_selected(_ssl, &proto, &protoLen);
  if (!proto || std::string(reinterpret_cast<char const*>(proto), protoLen) != "h2")
    KEXCEPT(Exception, "HTTP2: server did not negotiate h2 via ALPN, host: " + _host);

  sslWrite(frame::CONNECTION_PREFACE());
  std::vector<frame::settings::Entry> mySettings{{frame::settings::ENABLE_PUSH, 0}};
  sslWrite(frame::BUILD(frame::Type::SETTINGS, 0, 0, frame::settings::BUILD(mySettings)));

  _connected = true;
  return *this;
}

mkn::ram::http2::Response mkn::ram::http2::Client::request(Request const& req) {
  if (!_connected) connect();

  uint32_t const streamId = _nextStreamId;
  _nextStreamId += 2;

  hpack::Headers pseudo = {
      {":method", req.method()},
      {":scheme", req.scheme()},
      {":authority", req.host()},
      {":path", req.path()},
  };
  for (auto const& h : req.headers()) pseudo.emplace_back(h.first, h.second);

  std::string const headerBlock = hpack::Encoder::encode(pseudo);
  bool const hasBody = !req.body().empty();
  uint8_t const headersFlags = frame::flag::END_HEADERS | (hasBody ? 0 : frame::flag::END_STREAM);

  // Encoded request headers exceeding the peer's max frame size (needing CONTINUATION)
  // is not handled yet - fine for typical request header sizes; left as a follow-up.
  sslWrite(frame::BUILD(frame::Type::HEADERS, headersFlags, streamId, headerBlock));

  if (hasBody) {
    std::string const& body = req.body();
    size_t sent = 0;
    while (sent < body.size()) {
      size_t const chunk = std::min(static_cast<size_t>(_peerMaxFrameSize), body.size() - sent);
      bool const last = (sent + chunk) >= body.size();
      sslWrite(frame::BUILD(frame::Type::DATA, last ? frame::flag::END_STREAM : 0, streamId,
                            body.substr(sent, chunk)));
      sent += chunk;
    }
  }

  Response res;
  std::string headerFragment;
  bool streamDone = false;
  int64_t streamRecvConsumed = 0;

  while (!streamDone) {
    frame::Header const fh = readFrameHeader();
    std::string const payload = fh.length ? sslReadExact(fh.length) : std::string();

    switch (fh.type) {
      case frame::Type::SETTINGS:
        handleSettings(fh, payload);
        break;
      case frame::Type::PING:
        handlePing(fh, payload);
        break;
      case frame::Type::WINDOW_UPDATE:
        handleWindowUpdate(fh, payload);
        break;
      case frame::Type::HEADERS:
      case frame::Type::CONTINUATION: {
        std::string frag = payload;
        if (fh.type == frame::Type::HEADERS && (fh.flags & frame::flag::PADDED)) {
          if (frag.empty()) KEXCEPT(Exception, "HTTP2: malformed padded HEADERS frame");
          uint8_t const padLen = static_cast<uint8_t>(frag[0]);
          frag = frag.substr(1, frag.size() - 1 - padLen);
        }
        if (fh.type == frame::Type::HEADERS && (fh.flags & frame::flag::PRIORITY)) {
          if (frag.size() < 5) KEXCEPT(Exception, "HTTP2: malformed HEADERS frame (priority)");
          frag = frag.substr(5);
        }
        headerFragment += frag;
        if (fh.flags & frame::flag::END_HEADERS) {
          for (auto const& h : _decoder.decode(headerFragment)) {
            if (h.first == ":status")
              res.status(static_cast<uint16_t>(std::stoi(h.second)));
            else
              res.header(h.first, h.second);
          }
          headerFragment.clear();
        }
        if (fh.flags & frame::flag::END_STREAM) streamDone = true;
        break;
      }
      case frame::Type::DATA: {
        std::string data = payload;
        if (fh.flags & frame::flag::PADDED) {
          if (data.empty()) KEXCEPT(Exception, "HTTP2: malformed padded DATA frame");
          uint8_t const padLen = static_cast<uint8_t>(data[0]);
          data = data.substr(1, data.size() - 1 - padLen);
        }
        res.appendBody(data.data(), data.size());
        streamRecvConsumed += static_cast<int64_t>(fh.length);
        _connRecvConsumed += fh.length;
        maybeSendConnectionWindowUpdate();
        if (streamRecvConsumed > _MKN_RAM_HTTP2_INITIAL_WINDOW_SIZE_ / 2) {
          sslWrite(frame::BUILD(
              frame::Type::WINDOW_UPDATE, 0, streamId,
              frame::WINDOW_UPDATE_PAYLOAD(static_cast<uint32_t>(streamRecvConsumed))));
          streamRecvConsumed = 0;
        }
        if (fh.flags & frame::flag::END_STREAM) streamDone = true;
        break;
      }
      case frame::Type::RST_STREAM: {
        uint32_t const code =
            payload.size() >= 4 ? frame::READ_U32(reinterpret_cast<uint8_t const*>(payload.data()))
                                : 0;
        KEXCEPT(Exception, "HTTP2: stream reset by peer, error=" + std::to_string(code));
      }
      case frame::Type::GOAWAY: {
        auto const ga = frame::GoAway::PARSE(payload);
        KEXCEPT(Exception, "HTTP2: GOAWAY from peer, error=" + std::to_string(ga.error_code));
      }
      default:
        break;  // PRIORITY / PUSH_PROMISE (disabled via our ENABLE_PUSH=0) / unknown - ignore
    }
  }
  return res;
}

void mkn::ram::http2::Client::close() {
  if (_ssl) {
    SSL_shutdown(_ssl);
    SSL_free(_ssl);
    _ssl = nullptr;
  }
  if (_ctx) {
    SSL_CTX_free(_ctx);
    _ctx = nullptr;
  }
  if (_sck >= 0) {
    ::close(_sck);
    _sck = -1;
  }
  _connected = false;
}

#endif  //_MKN_RAM_INCLUDE_HTTP2_
