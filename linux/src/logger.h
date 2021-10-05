
#ifndef C6B3C6F0_EFD2_46C1_BD00_5AA4B69BDDCC
#define C6B3C6F0_EFD2_46C1_BD00_5AA4B69BDDCC
#include <Sys.h>
#include <context.h>
#include <etl/string.h>
#include <etl/string_stream.h>

#include <iomanip>
using cstr = const char *const;

static constexpr cstr past_last_slash(cstr str, cstr last_slash) {
  return *str == '\0' ? last_slash
                      : *str == '/' ? past_last_slash(str + 1, str + 1)
                                    : past_last_slash(str + 1, last_slash);
}

static constexpr cstr past_last_slash(cstr str) {
  return past_last_slash(str, str);
}
#ifndef __SHORT_FILE__
#define __SHORT_FILE__                                                         \
  ({                                                                           \
    constexpr cstr sf__{past_last_slash(__FILE__)};                            \
    sf__;                                                                      \
  })
#endif
#include <sstream>
String stringFormat(const char *fmt, ...);
String hexDump(Bytes, const char *spacer = " ");
String charDump(Bytes);
std::string time_in_HH_MM_SS_MMM();

#define ColorOrange "\033[33m"
#define ColorGreen "\033[32m"
#define ColorPurple "\033[35m"
#define ColorDefault "\033[39m"

extern struct endl {
} LEND;

class LogS {
  stringstream _ss;

public:
  typedef enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } Level;
  Level level;
  etl::format_spec format = etl::format_spec().width(20).fill('#');
  LogS() { level = LOG_INFO; };

  void operator<<(struct endl x) { flush(); }

  template <class T> LogS &operator<<(const T &t) {
    _ss << t;
    return *this;
  }

  template <class T> LogS &operator<<(String &t) {
    _ss << t.c_str();
    return *this;
  }

  void flush() {
    string s = _ss.str();
    fprintf(stdout, "%s\n", s.c_str());
    stringstream().swap(_ss);
  }

  void log(char level, const char *file, uint32_t line, const char *function,
           const char *fmt, ...) {
    *this << Sys::millis() << ' ' << level << file << ":" << line << fmt
          << LEND;
  }
  LogS &log(char level, const char *file, uint32_t line) {
    _ss << time_in_HH_MM_SS_MMM() << ' ' << level << ' ' << setw(20) << file
        << ":" << setw(4) << line << " | ";
    return *this;
  }
};

extern LogS logger;
#define LOGD                                                                   \
  if (logger.level <= LogS::LOG_DEBUG)                                         \
  logger.log('D', __SHORT_FILE__, __LINE__)
#define LOGI                                                                   \
  if (logger.level <= LogS::LOG_INFO)                                          \
  logger.log('I', __SHORT_FILE__, __LINE__)
#define LOGW                                                                   \
  if (logger.level <= LogS::LOG_WARN)                                          \
  logger.log('W', __SHORT_FILE__, __LINE__)
#define LOGE                                                                   \
  if (logger.level <= LogS::LOG_ERROR)                                         \
  logger.log('E', __SHORT_FILE__, __LINE__)
#define CHECK LOGI << " so far so good " << LEND

#ifdef INFO
#undef INFO
#undef WARN
#undef DEBUG
#undef ERROR
#error INFO defined
#endif
#define TRACE(fmt, ...) LOGD << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define DEBUG(fmt, ...) LOGD << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define INFO(fmt, ...) LOGI << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define WARN(fmt, ...) LOGW << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define ERROR(fmt, ...) LOGE << stringFormat(fmt, ##__VA_ARGS__) << LEND;

#endif /* C6B3C6F0_EFD2_46C1_BD00_5AA4B69BDDCC */
