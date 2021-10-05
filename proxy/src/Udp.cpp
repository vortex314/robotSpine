
#include <Udp.h>
#include <log.h>
#include <util.h>

int Udp::init() {
  struct sockaddr_in servaddr;
  if ((_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    return (errno);
  }

  int optval = 1;
  setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
             sizeof(int));
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET; // IPv4
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(_myPort);
  if (bind(_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind failed");
    return (errno);
  }
  return 0;
}

int Udp::deInit() {
  int rc = close(_sockfd);
  if (rc)
    perror("close failed");
  return rc;
}

int Udp::receive(UdpMsg &rxd) {
  struct sockaddr_in clientaddr;
  memset(&clientaddr, 0, sizeof(clientaddr));

  socklen_t len = sizeof(clientaddr); // len is value/resuslt
  clientaddr.sin_family = AF_INET;
  clientaddr.sin_port = htons(_myPort);
  clientaddr.sin_addr.s_addr = INADDR_ANY;

  int rc = recvfrom(_sockfd, (char *)buffer, sizeof(buffer), MSG_WAITALL,
                    (struct sockaddr *)&clientaddr, &len);

  if (rc >= 0) {
    rxd.message.clear();
    rxd.src.ip = clientaddr.sin_addr.s_addr;
    rxd.src.port = ntohs(clientaddr.sin_port);
    char strIp[100];
    inet_ntop(AF_INET, &(rxd.src.ip), strIp, INET_ADDRSTRLEN);

    INFO(" received from %s:%d \n", strIp, rxd.src.port);
    rxd.message = Bytes(buffer, buffer + rc);
    return 0;
  } else {
    return errno;
  }
}

// Client side implementation of UDP client-server model

// Driver code
int Udp::send(UdpMsg &udpMsg) {
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = udpMsg.dst.port;
  server.sin_addr.s_addr = udpMsg.dst.ip;

  int rc = sendto(_sockfd, udpMsg.message.data(), udpMsg.message.size(), 0,
                  (const struct sockaddr *)&server, sizeof(server));
  if (rc < 0)
    return errno;
  return 0;
}

/*
int main(int argc,char* argv[]) {
        Udp udp;
        udp.port(1883);
        UdpMsg udpMsg;
        udpMsg.dstIp("192.168.0.195");
        udpMsg.dstPort(4210);
        udpMsg.message=">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
"; udp.init(); udp.send(udpMsg); UdpMsg rcvMsg; udp.receive(rcvMsg);
        printf("received : '%s'\n",rcvMsg.message.c_str());
}
*/
#include <arpa/inet.h>
#include <errno.h>  //For errno - the error number
#include <netdb.h>  //hostent
#include <stdio.h>  //printf
#include <stdlib.h> //for exit(0);
#include <string.h> //memset
#include <sys/socket.h>
bool getInetAddr(in_addr_t &addr, std::string &hostname) {
  addr = (in_addr_t)(-1);
  if (isdigit(hostname[0])) { // IP dot notation
    addr = inet_addr(hostname.c_str());
  } else { // host name
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname.c_str(), "http", &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
      if (p->ai_addr->sa_family == AF_INET) {
        sockaddr *sa = p->ai_addr;
        addr = ((sockaddr_in *)sa)->sin_addr.s_addr;
      }
    }
    freeaddrinfo(servinfo); // all done with this structure
  }
  return addr != (in_addr_t)(-1);
}


bool getNumber(uint16_t &x, const string &s) {
  x = 0;
  for (char const &ch : s) {
    if (std::isdigit(ch)) {
      x *= 10;
      x += x - '0';
    } else {
      return false;
    }
  }
  return true;
}

bool UdpAddress::fromUri(UdpAddress &udpAddress, std::string uri) {
  auto parts = split(uri, ':');

  return parts.size() == 2 && getInetAddr(udpAddress.ip, parts[0]) &&
         getNumber(udpAddress.port, parts[1]);
}