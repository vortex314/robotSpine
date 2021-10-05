
#include "logger.h"
#include <assert.h>
#include <string.h>

std::string hexDump(Bytes bs, const char *spacer) {
  static char HEX_DIGITS[] = "0123456789ABCDEF";
  std::string out;
  for (uint8_t b : bs) {
    out += HEX_DIGITS[b >> 4];
    out += HEX_DIGITS[b & 0xF];
    out += spacer;
  }
  return out;
}

std::string charDump(Bytes bs) {
  std::string out;
  for (uint8_t b : bs) {
    if (isprint(b))
      out += (char)b;
    else if (b == '\n') {
      out += "\\n";
    } else if (b == '\r') {
      out += "\\r";
    } else
      out += '.';
  }
  return out;
}

#include <stdarg.h>
#include <string>
string stringFormat(const char *fmt, ...) {
  static std::string str;
  str.clear();
  int size = strlen(fmt) * 2 + 50; // Use a rubric appropriate for your code
  if (size > 10240)
    fprintf(stdout, " invalid log size\n");
  va_list ap;
  while (1) { // Maximum two passes on a POSIX system...
    assert(size < 10240);
    str.resize(size);
    va_start(ap, fmt);
    int n = vsprintf((char *)str.data(), fmt, ap);
    va_end(ap);
    if (n > -1 && n < size) { // Everything worked
      str.resize(n);
      return str.c_str();
    }
    if (n > -1)     // Needed size returned
      size = n + 1; // For null char
    else
      size *= 2; // Guess at a larger size (OS specific)
  }
  return str;
}

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
std::string time_in_HH_MM_SS_MMM() {
  using namespace std::chrono;

  // get current time
  auto now = system_clock::now();

  // get number of milliseconds for the current second
  // (remainder after division into seconds)
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  // convert to std::time_t in order to convert to std::tm (broken time)
  auto timer = system_clock::to_time_t(now);

  // convert to broken time
  std::tm bt = *std::localtime(&timer);

  std::ostringstream oss;

  oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

  return oss.str();
}