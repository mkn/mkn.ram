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
| `lib` | TCP, HTTP (default) |
| `https` | HTTPS, links OpenSSL (Conan on Windows) |
| `fcgi` | FastCGI |

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
