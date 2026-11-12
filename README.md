# mkn.ram

**C++ Remote Applications Management**

Cross platform TCP / HTTP / HTTPS clients and servers.

- [README.noformat](README.noformat): compile switches
- [tst/usage.cpp](tst/usage.cpp): servers and clients for each protocol

## Example

```cpp
#include "mkn/ram/http.hpp"

class Server : public mkn::ram::http::Server {
 public:
  Server() : mkn::ram::http::Server(8888) {}
  mkn::ram::http::_1_1Response respond(mkn::ram::http::A1_1Request const&) override {
    mkn::ram::http::_1_1Response r;
    return r.withBody("hello").withDefaultHeaders();
  }
};
```

## Profiles

| profile | adds |
|---|---|
| `lib` | TCP, HTTP, FastCGI (default) |
| `https` | HTTPS, links OpenSSL (Conan on Windows) |

## FastCGI

`mkn/ram/fcgi.hpp` has FastCGI responder servers to run behind a web server such as nginx.
Handlers take and return the http types, so one handler can serve http, https and FastCGI.
POSIX only.

- `fcgi::Server` handles one request at a time on its connection thread
- `fcgi::AsyncServer` hands requests to a worker pool and multiplexes connections

```cpp
mkn::ram::fcgi::AsyncServer server(9000, /*workers=*/4);  // binds 127.0.0.1 by default
server.withResponse([](mkn::ram::http::A1_1Request const& r) {
  return mkn::ram::http::Response{}.withBody("hello " + r.path()).withDefaultHeaders();
});
server.start();  // non-blocking, see stop() and join()
```

```nginx
location / {
  include fastcgi_params;
  fastcgi_pass 127.0.0.1:9000;
  fastcgi_keep_conn on;
}
```

`res/test/nginx.sh` runs both servers behind nginx, see the script for how to build and run it.

## Usage

In a project `mkn.yaml`:

```yaml
dep: mkn.ram[https]
```

Build and run the tests:

```bash
mkn clean build -dtSa "-O2 -fPIC" -p test -l -pthread
```

## Dependencies

[mkn.kul](https://github.com/mkn/mkn.kul), OpenSSL for `https`
