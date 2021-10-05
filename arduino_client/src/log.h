
#ifndef C6B3C6F0_EFD2_46C1_BD00_5AA4B69BDDCC
#define C6B3C6F0_EFD2_46C1_BD00_5AA4B69BDDCC
#include <Arduino.h>
#include <Sys.h>
#include <context.h>
#include <etl/string.h>
#include <etl/string_stream.h>

//#include <iomanip>
using cstr = const char *const;

static constexpr cstr past_last_slash(cstr str, cstr last_slash)
{
  return *str == '\0'  ? last_slash
         : *str == '/' ? past_last_slash(str + 1, str + 1)
                       : past_last_slash(str + 1, last_slash);
}

static constexpr cstr past_last_slash(cstr str)
{
  return past_last_slash(str, str);
}
#ifndef __SHORT_FILE__
#define __SHORT_FILE__                                  \
  (                                                     \
      {                                                 \
        constexpr cstr sf__{past_last_slash(__FILE__)}; \
        sf__;                                           \
      })
#endif
std::string stringFormat(const char *fmt, ...);
std::string hexDump(Bytes, const char *spacer = " ");
std::string charDump(Bytes);

#undef LEND
extern struct endl
{
} LEND;

class LogS
{
  etl::string<256> _str;
  etl::string_stream &_ss;

public:
  typedef enum
  {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
  } Level;
  Level level;
  etl::format_spec format = etl::format_spec().width(20).fill('#');
  LogS() : _ss(*new etl::string_stream(_str)) { level = LOG_INFO; };

  inline void operator<<(struct endl x)
  {
    flush();
  }

  template <class T>
  LogS &operator<<(const T &t)
  {
    _ss << t;
    return *this;
  }

  LogS &operator<<(std::string &t)
  {
    _ss << t.c_str();
    return *this;
  }

    LogS &operator<<( std::string t)
  {
    _ss << t.c_str();
    return *this;
  }

  LogS &operator<<(const char* t)
  {
    _ss << t;
    return *this;
  }

  void flush()
  {
    Serial.println(_str.c_str());
    _str = "";
  }

  LogS &log(const char *level, const char *file, uint32_t line)
  {
    _ss << Sys::millis() << level << etl::setw(10) << file << ":"
        << etl::setw(4) << line << " | ";
    return *this;
  }
};

extern LogS logger;
#define LOGD                           \
  if (logger.level <= LogS::LOG_DEBUG) \
  logger.log(" D ", __SHORT_FILE__, __LINE__)
#define LOGI                          \
  if (logger.level <= LogS::LOG_INFO) \
  logger.log(" I ", __SHORT_FILE__, __LINE__)
#define LOGW                          \
  if (logger.level <= LogS::LOG_WARN) \
  logger.log(" W ", __SHORT_FILE__, __LINE__)
#define LOGE                           \
  if (logger.level <= LogS::LOG_ERROR) \
  logger.log(" E ", __SHORT_FILE__, __LINE__)
#define CHECK LOGI << " so far so good " << LEND

#ifdef INFO
#undef INFO
#undef WARN
#undef DEBUG
#undef ERROR
#error INFO defined
#endif
/*
#define DEBUG(fmt, ...) { Serial.print(__FILE__ );Serial.print(":");Serial.print(__LINE__);Serial.println(fmt);}
#define INFO(fmt, ...) { Serial.print(__FILE__ );Serial.print(":");Serial.print(__LINE__);Serial.println(fmt);}
#define WARN(fmt, ...) { Serial.print(__FILE__ );Serial.print(":");Serial.print(__LINE__);Serial.println(fmt);}
#define ERROR(fmt, ...) { Serial.print(__FILE__ );Serial.print(":");Serial.print(__LINE__);Serial.println(fmt);}
*/


#define DEBUG(fmt, ...) LOGD << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define INFO(fmt, ...) LOGI << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define WARN(fmt, ...) LOGW << stringFormat(fmt, ##__VA_ARGS__) << LEND;
#define ERROR(fmt, ...) LOGE << stringFormat(fmt, ##__VA_ARGS__) << LEND;

#endif /* C6B3C6F0_EFD2_46C1_BD00_5AA4B69BDDCC */
