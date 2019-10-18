# reverse-tunnel-cpp
A Small program punch through NAT firewalls

This project is only a proof of concept, which is not suitable for running for a long period.
Please use [frp](https://github.com/fatedier/frp). It is better documented, and the features are much more richer than this project.

The command option are:
```
    ("help,h",    "Print this help messages")
    ("srv",       po::value<std::string>(&srv_listen_host)->default_value(":7000"), "[server mode] listen port")
    ("connect,c", po::value<std::string>(), "[export mode] connect to server")
    ("export,e",  po::value<std::string>(), "[export mode] export server endpoint")
    ("bind,b",    po::value<std::string>(), "[export mode] bind remote server")
    ("socks5,s",  po::value<std::string>(), "[socks5 mode] start socks5 server on this port");
```

For example:
```
./reverse-tunnel --srv :7000
```
Running reverse-tunnel on port :7000

```
./reverse-tunnel --connect 127.0.0.1:7000 --bind :8000 --export localhost:8080
```
connect to remote server `127.0.0.1:7000`, request to bind on `:8000` on the remote server, and export my `localhost:8080` service.
