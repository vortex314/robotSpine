#ifndef __ReflectFromCbor__
#define __ReflectFromCbor__
#include <CborDeserializer.h>

class ReflectFromCbor
{
  CborDeserializer &_cborDeserializer;

public:
  ReflectFromCbor(size_t size) : _cborDeserializer(*new CborDeserializer(size)){};
  ~ReflectFromCbor() { delete &_cborDeserializer; };
  ReflectFromCbor &begin()
  {
    _cborDeserializer.begin();
    return *this;
  };
  ReflectFromCbor &end()
  {
    _cborDeserializer.end();
    return *this;
  };
  template <typename T>
  ReflectFromCbor &member(T &t, const char *name = "", const char *desc = "")
  {
    _cborDeserializer >> t;
    return *this;
  }
  ReflectFromCbor &fromBytes(const Bytes &bs)
  {
    _cborDeserializer.fromBytes(bs);
    return *this;
  }
  bool success() { return _cborDeserializer.success(); }
};
#endif