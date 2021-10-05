#include <context.h>
#include <etl/string.h>
#include <etl/string_stream.h>

class ReflectToDisplay {
#ifdef __ARDUINO__
  etl::string<256> _text;
  etl::string_stream _ss(_text);
#else
  stringstream _ss;
#endif

 public:
  template <typename T>
  ReflectToDisplay &member(T &t, const char *n, const char *desc) {
    _ss << n << ":" << t << ",";
    return *this;
  }
  ReflectToDisplay &member(String &t, const char *name, const char *desc) {
    _ss << name << ":" << t.c_str() << ",";
    return *this;
  }
  ReflectToDisplay &member(Bytes &t, const char *name, const char *desc) {
    _ss << name << ":" << hexDump(t).c_str() << ",";
    return *this;
  }
  String toString() {
#ifdef __ARDUINO__
    return String(_text.c_str());
#else
    return _ss.str();
#endif
  }
  ReflectToDisplay &begin() {
#ifdef __ARDUINO__
    _text = "";
#else
    stringstream().swap(_ss);
#endif
    return *this;
  }
  ReflectToDisplay &end() { return *this; }
};