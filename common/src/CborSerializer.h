#ifndef __CBOR_SERIALIZER_H__
#define __CBOR_SERIALIZER_H__
#include <assert.h>
#include <cbor.h>
#include <context.h>
#include <log.h>

using namespace std;

class CborSerializer
{
  uint32_t _capacity;
  size_t _size;
  CborError _err;
  CborEncoder _encoderRoot;
  CborEncoder _encoder;
  uint8_t *_buffer;

public:
  CborSerializer(int size);
  ~CborSerializer();
  template <typename T>
  CborSerializer &operator<<(T t)
  {
    return add(t);
  }
  // CborSerializer &addRaw(const Bytes& );

  CborSerializer &add(int t);
  //  CborSerializer &add(int32_t &t);
  CborSerializer &add(uint32_t t);
  CborSerializer &add(int64_t t);
  CborSerializer &add(uint64_t t);
  CborSerializer &add(std::string t);
  CborSerializer &add(float t);
  CborSerializer &add(double t);
  CborSerializer &add(Bytes t);
  CborSerializer &begin();
  CborSerializer &end();
  bool success();
  Bytes toBytes();
};
#endif /* E04F2BFB_223A_4990_A609_B7AA6A5E6BE8 */
