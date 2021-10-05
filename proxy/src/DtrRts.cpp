#include <errno.h>
#include <fcntl.h>
#include <log.h>
#include <stdio.h>
#include <string.h>     // for strerror
#include <sys/ioctl.h>  //ioctl() call defenitions
#include <unistd.h>     // for close

#include <string>
typedef vector<uint8_t> Bytes;

LogS logger;

/*
    /DTR    /RTS   /RST  GPIO0   TIOCM_DTR    TIOCM_RTS
    1       0       0       1       0           1           => reset
    0       1       1       0       1           0           => prog enable
    1       1       1       1       0           0           => run
    0       0       1       1       1           1           => run

to run mode
    run => reset => run
    run => prog enable => reset => run

*/

#define ASSERT(xx) \
  if (!(xx)) WARN(" failed " #xx " %d : %s ", errno, strerror(errno));

int modeInfo(int fd, const char* comment = "") {
  int mode;
  int rc = ioctl(fd, TIOCMGET, &mode);
  ASSERT(rc == 0);
  std::string sMode;
  if (mode & TIOCM_CAR) sMode += "TIOCM_CAR,";
  if (mode & TIOCM_CD) sMode += "TIOCM_CD,";
  if (mode & TIOCM_DTR) sMode += "TIOCM_DTR,";
  if (mode & TIOCM_RTS) sMode += "TIOCM_RTS,";
  if (mode & TIOCM_DSR) sMode += "TIOCM_DSR,";
  INFO("%s line mode : %s", comment, sMode.c_str());
  fflush(stdout);
  return 0;
}

static bool setBit(int fd, int flags) {
  int rc = ioctl(fd, TIOCMBIS, &flags);  // set DTR pin
  if (rc) WARN("ioctl()= %s (%d)", strerror(errno), errno);
  return rc == 0;
}
static bool clrBit(int fd, int flags) {
  int rc = ioctl(fd, TIOCMBIC, &flags);
  if (rc) WARN("ioctl()= %s (%d)", strerror(errno), errno);
  return rc == 0;
}

bool setBits(int fd, int flags) {
  int rc = ioctl(fd, TIOCMSET, &flags);  // set DTR pin
  if (rc) WARN("ioctl()= %s (%d)", strerror(errno), errno);
  return rc == 0;
}
/*
to toggle RESET pin RTS pin should be opposite DTR
*/
void reset(int fd) {
  int mode;
  int rc = ioctl(fd, TIOCMGET, &mode);
  ASSERT(rc == 0)
  if (mode & TIOCM_DTR) {
    sleep(1);
    clrBit(fd, TIOCM_RTS);  // raises RTS pin
    sleep(1);
    setBit(fd, TIOCM_RTS);  // lowers RTS pin
  } else {
    sleep(1);
    setBit(fd, TIOCM_RTS);  // lowers RTS pin
    sleep(1);
    clrBit(fd, TIOCM_RTS);  // raises RTS pin
  }
}

void setDtr(int fd) { setBit(fd, TIOCM_DTR); }
void clrDtr(int fd) { clrBit(fd, TIOCM_DTR); }

void setRts(int fd) { setBit(fd, TIOCM_RTS); }
void clrRts(int fd) { clrBit(fd, TIOCM_RTS); }

void resetLow(int fd) { setBits(fd, TIOCM_RTS); }
void progLow(int fd) { setBits(fd, TIOCM_DTR); }
void allHigh(int fd) { setBits(fd, 0); }

class EspTool {
  const uint8_t ESP_SYNC = 0x08;  // sync char
  const uint8_t ESP_NONE = 0x00;
  const uint32_t SYNC_TIMEOUT_MS = 100;
  static const int ESP_CHECKSUM_MAGIC = 0xEF;
  static const int DEFAULT_TIMEOUT = 3;
  static const uint32_t CHIP_ERASE_TIMEOUT = 120;
  static const uint32_t MAX_TIMEOUT = CHIP_ERASE_TIMEOUT * 2;
  static const Bytes empty;
  static const int None = 0;
  static const int DEFAULT_CONNECT_ATTEMPTS =
      7;  //# default number of times to try connection

  int fd;
  uint32_t _timeout;
  typedef enum { default_reset } Mode;

 public:
  EspTool(int f) : fd(f){};

  Bytes toSlip(Bytes& data) {
    Bytes result;
    result.push_back(0xC0);
    //  for (auto b : data)
    for (auto it = data.begin(); it != data.end(); it++) {
      uint8_t b = *it;
      if (b == 0xDB) {
        result.push_back(0xDB);
        result.push_back(0xDD);
      } else if (b == 0xC0) {
        result.push_back(0xDB);
        result.push_back(0xDC);
      } else {
        result.push_back(b);
      }
    }
    result.push_back(0xC0);
    return result;
  }

  void write(Bytes packet) {
    Bytes buf = toSlip(packet);
    ::write(fd, buf.data(), buf.size());
  }

  Bytes toPacket(uint8_t op, Bytes data, uint8_t chk = 0) {
    Bytes result = {'<', 'B', 'B', 'H', 'I', 0x00, op, (uint8_t)data.size(),
                    chk};
    for (uint8_t b : data) result.push_back(b);
    return result;
  }

  Bytes command(uint8_t op = None, Bytes bs = Bytes(),
                uint32_t timeout = DEFAULT_TIMEOUT) {
    uint32_t saved_timeout = _timeout;
    uint32_t new_timeout = timeout < MAX_TIMEOUT ? timeout : MAX_TIMEOUT;
    uint8_t buffer[1024];
    uint64_t startTime = Sys::millis();
    Bytes packet = toPacket(op, bs);
    INFO("FLUSH");
    int cnt;
    while (cnt = read(fd, buffer, sizeof(buffer)) > 0) {
      INFO("FLUSH %d ", cnt);
    };  // flush input
    INFO("WRITE %s", hexDump(packet).c_str());
    INFO("WRITE %s", charDump(packet).c_str());

    write(packet);
    while (startTime + timeout > Sys::millis()) {
      INFO("READ");
      int count = read(fd, buffer, sizeof(buffer));
      if (count > 0) {
        Bytes result(buffer, buffer + count);
        INFO("RXD %s == %s ", hexDump(result).c_str(),
             charDump(result).c_str());
        return result;
      }
      usleep(1000);
    }
    return Bytes();
  }

  bool sync() {
    Bytes syncCommand = {0x07, 0x07, 0x12, 0x20};
    for (int i = 0; i < 32; i++) syncCommand.push_back(0x55);
    Bytes result = command(ESP_SYNC, syncCommand, SYNC_TIMEOUT_MS);
    for (int i = 0; i < 7; i++) {
      command();
    }
    return result.size() > 0 && result[0] != 0x80;
  }

  void progUc(int fd, bool esp32r0Delay = false) {
    clrDtr(fd);  // GPIO0 = 1
    setRts(fd);  // NRST = 0
    usleep(100000);
    if (esp32r0Delay) usleep(1200000);
    clrRts(fd);  // NRST =1
    setDtr(fd);  // GPIO0 = 0
    usleep(40000);
    if (esp32r0Delay) usleep(50000);
    clrDtr(fd);  // GPIO0 = 1
  }

  void runUc(int fd) {
    clrDtr(fd);
    setRts(fd);
    usleep(200000);
    clrRts(fd);
    usleep(200000);
  }

  void slip_reader(int fd) {}

  void flush_input(int fd) { slip_reader(fd); }

  void bootloader_reset(bool esp32r0_delay) {
    clrDtr(fd);  // IO0=HIGH
    setRts(fd);  //   # EN=LOW, chip in reset
    usleep(100000);
    if (esp32r0_delay) usleep(1200000);
    setDtr(fd);  // #IO0 = LOW
    clrRts(fd);  // #EN = HIGH,
    if (esp32r0_delay) usleep(400000);
    usleep(50000);
    clrDtr(fd);  //  #IO0 = HIGH,
  }

  int connect_attempt(bool esp32r0_delay) {
    bootloader_reset(esp32r0_delay);
    for (int i = 0; i < 5; i++) {
      flush_input(fd);
      fsync(fd);
      if (sync() == 0) return 0;
    }
    fprintf(stdout, esp32r0_delay ? "_" : ".");
    fflush(stdout);
    usleep(50000);
    return EIO;
  }

  bool connect(Mode mode = default_reset,
               int attempts = DEFAULT_CONNECT_ATTEMPTS,
               bool detecting = false) {
    int rc;
    bool esp32r0_delay = false;
    fprintf(stdout, "Connecting..");
    fflush(stdout);
    for (int i = 0; i < attempts; i++) {
      if (rc = connect_attempt(esp32r0_delay) == 0) return true;
      esp32r0_delay = true;
      if (rc = connect_attempt(esp32r0_delay) == 0) return true;
    }
    fprintf(stdout, "\n");
    if (rc) WARN("Failed to connect.");

    return false;
  }
};

const Bytes EspTool::empty;

int main(int argc, char** argv) {
  int fd;
  INFO("open %s ...", argv[1]);
  fd = open(argv[1], O_EXCL | O_RDWR | O_NOCTTY | O_NDELAY |
                         O_SYNC);  // Open Serial Port
  ASSERT(fd > 0);
  int flags = fcntl(fd, F_GETFL, 0);
  int rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  EspTool esp(fd);
  esp.connect();

  close(fd);
  return 0;
}