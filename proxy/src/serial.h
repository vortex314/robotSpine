#ifndef _SERIAL_H
#define _SERIAL_H

// For convenience
#include <log.h>
#include <asm-generic/ioctls.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <deque>
#include <fstream>
#include <map>
#include <vector>



using namespace std;

typedef vector<uint8_t> bytes;

struct SerialStats {
  uint64_t connectionCount = 0ULL;
  uint64_t connectionErrors = 0ULL;
  uint64_t messagesSent = 0ULL;
};

class Serial {
  string _port;       // /dev/ttyUSB0
  string _portShort;  // USB0
  int _baudrate;
  int _fd = 0;
  uint8_t _separator;
  bool _connected = false;

 public:
  Serial();
  ~Serial();
  // commands
  int init();
  int connect();
  int disconnect();
  int rxd(bytes &buffer);
  int txd(const bytes &);

  bool modeRun();
  bool modeProgram();
  int modeInfo();

  // properties
  int baudrate(uint32_t);
  uint32_t baudrate();
  int port(string);
  const string &port();
  int separator(uint8_t);
  int fd();
  bool connected() { return _connected; }

  const string shortName(void) const;
};

#endif
