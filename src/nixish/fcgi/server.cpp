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
#include "mkn/ram/fcgi.hpp"
#include "mkn/kul/log.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef MSG_NOSIGNAL  // macOS, see SO_NOSIGPIPE
#define MSG_NOSIGNAL 0
#endif

namespace mkn::ram::fcgi {
namespace {

void nonblocking(int const fd) {
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0)
    KEXCEPT(Exception, "fcgi fcntl failed: ", strerror(errno));
}

struct Pending {
  std::string params, in;
  bool keep = false, paramsDone = false, inDone = false;
};

struct Connection {
  int fd = -1;
  record::Parser parser;
  std::string out;
  std::unordered_map<std::uint16_t, Pending> pending;  // receiving
  std::unordered_map<std::uint16_t, bool> inflight;    // dispatched, value is keep
  bool closing = false;  // close once out is flushed and nothing is inflight
};

}  // namespace

struct AServer::Impl {
  std::string addr;
  std::uint16_t port;
  int lisock = -1, wake[2] = {-1, -1};
  std::thread thread;
  std::atomic<bool> run{false};

  mutable std::mutex mutex;  // guards done and ep
  std::vector<std::tuple<std::uint64_t, std::uint16_t, std::string>> done;
  std::exception_ptr ep;

  std::uint64_t next = 0;
  std::unordered_map<std::uint64_t, Connection> conns;  // loop thread only

  Impl(std::uint16_t p, std::string a) : addr(std::move(a)), port(p) {}
  ~Impl() {
    for (int const fd : {lisock, wake[0], wake[1]})
      if (fd >= 0) ::close(fd);
  }

  void notify() {
    char const c = 0;
    [[maybe_unused]] auto const n = ::write(wake[1], &c, 1);  // a full pipe is already awake
  }
};

AServer::AServer(std::uint16_t port, std::string addr)
    : _impl(std::make_unique<Impl>(port, std::move(addr))) {}

AServer::~AServer() {
  stop();
  join();
}

std::uint16_t AServer::port() const { return _impl->port; }

std::exception_ptr AServer::exception() const {
  std::lock_guard<std::mutex> lock(_impl->mutex);
  return _impl->ep;
}

AServer& AServer::init() {
  auto& m = *_impl;
  if (m.lisock >= 0) return *this;

  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(m.port);
  if (inet_pton(AF_INET, m.addr.c_str(), &sa.sin_addr) != 1)
    KEXCEPT(Exception, "fcgi invalid IPv4 address: " + m.addr);

  int const fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) KEXCEPT(Exception, "fcgi socket failed: ", strerror(errno));
  int const on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0 || ::listen(fd, 256) < 0) {
    auto const e = errno;
    ::close(fd);
    KEXCEPT(Exception, "fcgi bind/listen failed on ", m.addr, ":", m.port, ": ", strerror(e));
  }
  socklen_t len = sizeof(sa);
  getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &len);
  m.port = ntohs(sa.sin_port);
  nonblocking(fd);
  m.lisock = fd;

  if (pipe(m.wake) < 0) KEXCEPT(Exception, "fcgi pipe failed: ", strerror(errno));
  nonblocking(m.wake[0]);
  nonblocking(m.wake[1]);
  return *this;
}

AServer& AServer::start() {
  init();
  auto& m = *_impl;
  if (m.run.exchange(true)) return *this;
  m.thread = std::thread([this]() {
    try {
      loop();
    } catch (...) {
      std::lock_guard<std::mutex> lock(_impl->mutex);
      _impl->ep = std::current_exception();
    }
    for (auto& [key, conn] : _impl->conns) ::close(conn.fd);
    _impl->conns.clear();
    _impl->run = false;
  });
  return *this;
}

AServer& AServer::stop() {
  if (_impl->run.exchange(false)) _impl->notify();
  return *this;
}

AServer& AServer::join() {
  if (_impl->thread.joinable() && _impl->thread.get_id() != std::this_thread::get_id())
    _impl->thread.join();
  return *this;
}

http::Response AServer::respondSafely(http::A1_1Request const& req) noexcept {
  try {
    return respond(req);
  } catch (mkn::kul::Exception const& e) {
    KERR << e.stack();
  } catch (std::exception const& e) {
    KERR << e.what();
  } catch (...) {
    KERR << "fcgi responder threw an unknown exception";
  }
  http::Response res;
  res.status(500);
  res.reason("Internal Server Error");
  return res;
}

void AServer::complete(std::uint64_t const conn, std::uint16_t const id,
                       http::Response const& res) {
  auto out = record::respond(id, res);
  {
    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->done.emplace_back(conn, id, std::move(out));
  }
  _impl->notify();
}

void AServer::loop() {
  auto& m = *_impl;

  auto const flush = [](Connection& c) {
    while (!c.out.empty()) {
      auto const n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
      if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;
        return false;
      }
      c.out.erase(0, static_cast<std::size_t>(n));
    }
    return !(c.closing && c.inflight.empty());
  };

  auto const handle = [&](std::uint64_t const key, Connection& c, record::Record& r) {
    if (r.id == NULL_REQUEST_ID) {  // management
      if (r.type == type::GET_VALUES) {
        Params values;
        for (auto const& [k, v] : record::decode(r.content)) {
          if (k == "FCGI_MAX_CONNS" || k == "FCGI_MAX_REQS")
            values[k] = std::to_string(MKN_RAM_FCGI_MAX_CONNS);
          else if (k == "FCGI_MPXS_CONNS")
            values[k] = multiplexed() ? "1" : "0";
        }
        record::append(c.out, type::GET_VALUES_RESULT, 0, record::encode(values));
      } else {
        char const body[8] = {static_cast<char>(r.type), 0, 0, 0, 0, 0, 0, 0};
        record::append(c.out, type::UNKNOWN_TYPE, 0, {body, sizeof(body)});
      }
      return;
    }

    if (r.type == type::BEGIN_REQUEST) {
      if (r.content.size() < 8) KEXCEPT(Exception, "fcgi BEGIN_REQUEST too short");
      if (c.pending.count(r.id) || c.inflight.count(r.id)) return;  // already active
      auto const role = static_cast<std::uint16_t>((std::uint8_t(r.content[0]) << 8) |
                                                   std::uint8_t(r.content[1]));
      if (role != role::RESPONDER)
        record::end(c.out, r.id, status::UNKNOWN_ROLE);
      else if (!multiplexed() && (!c.pending.empty() || !c.inflight.empty()))
        record::end(c.out, r.id, status::CANT_MPX_CONN);
      else
        c.pending[r.id].keep = r.content[2] & KEEP_CONN;
      return;
    }

    auto const it = c.pending.find(r.id);
    if (it == c.pending.end()) return;  // records for inactive requests are ignored
    auto& p = it->second;

    if (r.type == type::ABORT_REQUEST) {
      record::end(c.out, r.id, status::REQUEST_COMPLETE);
      if (!p.keep) c.closing = true;
      c.pending.erase(it);
      return;
    }
    if (r.type == type::PARAMS && !p.paramsDone) {
      if (r.content.empty())
        p.paramsDone = true;
      else
        p.params += r.content;
    } else if (r.type == type::STDIN && !p.inDone) {
      if (r.content.empty())
        p.inDone = true;
      else
        p.in += r.content;
    }

    if (p.paramsDone && p.inDone) {
      auto req = std::make_shared<Request>(record::decode(p.params), std::move(p.in));
      c.inflight[r.id] = p.keep;
      c.pending.erase(it);
      dispatch(key, r.id, std::move(req));
    }
  };

  std::vector<pollfd> fds;
  std::vector<std::uint64_t> keys;
  std::vector<char> buf(MKN_RAM_FCGI_READ_BUFFER);

  while (m.run) {
    fds.clear();
    keys.clear();
    fds.push_back({m.wake[0], POLLIN, 0});
    fds.push_back({m.conns.size() < MKN_RAM_FCGI_MAX_CONNS ? m.lisock : -1, POLLIN, 0});
    for (auto const& [key, c] : m.conns) {
      short events = c.closing ? 0 : POLLIN;
      if (!c.out.empty()) events |= POLLOUT;
      fds.push_back({c.fd, events, 0});
      keys.push_back(key);
    }

    if (::poll(fds.data(), fds.size(), -1) < 0) {
      if (errno == EINTR) continue;
      KEXCEPT(Exception, "fcgi poll failed: ", strerror(errno));
    }
    if (!m.run) break;

    std::vector<std::uint64_t> closed;

    if (fds[0].revents) {
      char drain[64];
      while (::read(m.wake[0], drain, sizeof(drain)) > 0) {
      }
    }
    {
      decltype(m.done) done;
      {
        std::lock_guard<std::mutex> lock(m.mutex);
        done.swap(m.done);
      }
      for (auto& [key, id, out] : done) {
        auto const it = m.conns.find(key);
        if (it == m.conns.end()) continue;  // closed while responding
        auto& c = it->second;
        c.out += out;
        if (auto const f = c.inflight.find(id); f != c.inflight.end()) {
          if (!f->second) c.closing = true;
          c.inflight.erase(f);
        }
        if (!flush(c)) closed.push_back(key);
      }
    }

    if (fds[1].revents & POLLIN) {
      while (m.conns.size() < MKN_RAM_FCGI_MAX_CONNS) {
        int const fd = ::accept(m.lisock, nullptr, nullptr);
        if (fd < 0) break;  // EAGAIN, or a connection that died before accept
        try {
          nonblocking(fd);
#ifdef SO_NOSIGPIPE
          int const on = 1;
          setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
        } catch (Exception const& e) {
          KERR << e.stack();
          ::close(fd);
          continue;
        }
        m.conns[m.next++].fd = fd;
      }
    }

    for (std::size_t i = 0; i < keys.size(); ++i) {
      auto const revents = fds[i + 2].revents;
      if (!revents) continue;
      auto const it = m.conns.find(keys[i]);
      if (it == m.conns.end()) continue;
      auto& c = it->second;
      bool open = true;
      if (revents & (POLLIN | POLLHUP | POLLERR)) {
        while (open) {
          auto const n = ::recv(c.fd, buf.data(), buf.size(), 0);
          if (n > 0) {
            c.parser.feed(buf.data(), static_cast<std::size_t>(n));
            try {
              record::Record r;
              while (c.parser.next(r)) handle(keys[i], c, r);
            } catch (mkn::kul::Exception const& e) {
              KERR << e.stack();
              open = false;
            }
          } else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
            open = false;  // the peer closed, or the connection failed
          else
            break;
        }
      }
      if (open) open = flush(c);
      if (!open) closed.push_back(keys[i]);
    }

    for (auto const key : closed)
      if (auto const it = m.conns.find(key); it != m.conns.end()) {
        ::close(it->second.fd);
        m.conns.erase(it);
      }
  }
}

}  // namespace mkn::ram::fcgi
