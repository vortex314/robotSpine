#ifndef __ReflectToCbor_H__
#define __ReflectToCbor_H__

#include <CborSerializer.h>

using namespace std;

class ReflectToCbor
{
  CborSerializer& _cborSerializer;

public:
  ReflectToCbor(int size) : _cborSerializer(* new CborSerializer(size)){};
  template <typename T>
  ReflectToCbor &member(const T &t, const char *name = "", const char *desc = "")
  {
    _cborSerializer << t;
    return *this;
  }

  ReflectToCbor &begin()
  {
    _cborSerializer.begin();
    return *this;
  };
  ReflectToCbor &end()
  {
    _cborSerializer.end();
    return *this;
  };
  Bytes toBytes() { return _cborSerializer.toBytes(); };
  ~ReflectToCbor(){};
};
#endif /* E04F2BFB_223A_4990_A609_B7AA6A5E6BE8 */
