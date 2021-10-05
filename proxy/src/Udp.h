// Server side implementation of UDP client-server model
#ifndef UDP_H
#define UDP_H
#include <arpa/inet.h>
#include <context.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define MAXLINE 1500

template <typename T> struct Try {
public:
  int rc;
  T t;
  static Try Success(T t) {
    Try tr = {0, t};
    return tr;
  }
  static Try Fail(int err) {
    Try tr;
    tr.rc = err;
    return tr;
  }
  bool failed() { return rc != 0; }
  bool success() { return rc != 0; }
};

struct UdpAddress {
  in_addr_t ip;
  uint16_t port;
  bool operator==(UdpAddress &other) {
    return other.ip == ip && other.port == port;
  }
  static bool fromUri(UdpAddress& ,std::string);
};

struct UdpMsg {
public:
  UdpAddress src;
  UdpAddress dst;
  Bytes message;
  void dstIpString(const char *ip) { dst.ip = inet_addr(ip); }
  void dstPort(uint16_t port) { dst.port = htons(port); }
};

class Udp {
  uint16_t _myPort = 1883;
  int _sockfd;
  char buffer[MAXLINE];

public:
  void dst(const char *);
  void port(uint16_t port) { _myPort = port; }
  int init();
  int receive(UdpMsg &);
  int send(UdpMsg &);
  int fd() { return _sockfd; };
  int deInit();
};
#endif
