#include "CborSerializer.h"

#undef assert
#define assert(xxx) \
  if (!(xxx))       \
    LOGW << " assert failed " << #xxx << LEND;

CborSerializer::CborSerializer(int size)
{
  _buffer = new uint8_t[size];
  _capacity = size;
  //    cbor_encoder_init(&_encoderRoot, _buffer, _capacity, 0);
  //   cbor_encoder_create_array(&_encoderRoot, &_encoder,
  //   CborIndefiniteLength);
}
CborSerializer::~CborSerializer() { delete[] _buffer; }
CborSerializer &CborSerializer::add(const int t)
{
  _err = cbor_encode_int(&_encoder, t);
  assert(_err == 0);
  return *this;
}
CborSerializer &CborSerializer::add(uint32_t t)
{
  _err = cbor_encode_uint(&_encoder, t);
  assert(_err == 0);
  return *this;
}
CborSerializer &CborSerializer::add(uint64_t t)
{
  _err = cbor_encode_uint(&_encoder, t);
  assert(_err == 0);
  return *this;
}
CborSerializer &CborSerializer::add(int64_t t)
{
  _err = cbor_encode_int(&_encoder, t);
  assert(_err == 0);
  return *this;
}
CborSerializer &CborSerializer::add(std::string t)
{
  _err = cbor_encode_text_string(&_encoder, t.c_str(), t.length());
  assert(_err == 0);
  return *this;
}
/*
CborSerializer &CborSerializer::add(const std::string t) {
  _err = cbor_encode_text_string(&_encoder, t.c_str(), t.length());
  return *this;
}
CborSerializer &CborSerializer::add(int t) {
  LOGI << " encoding int " << LEND;

  _err = cbor_encode_int(&_encoder, t);
  assert(_err == 0);
  return *this;
}

CborSerializer &CborSerializer::add(const Bytes &t) {
  return *this << (Bytes &)t;
}
*/
CborSerializer &CborSerializer::add(double t)
{
  if ( !_err);
  _err = cbor_encode_double(&_encoder, t);
  assert(_err == 0);
  return *this;
}

CborSerializer &CborSerializer::add(Bytes t)
{
  if ( !_err)
  _err = cbor_encode_byte_string(&_encoder, t.data(), t.size());
  if (_err)
    LOGW << "cbor error " << _err << LEND;
  assert(_err == 0);
  return *this;
}

CborSerializer &CborSerializer::begin()
{
  _err = CborNoError;
  cbor_encoder_init(&_encoderRoot, _buffer, _capacity, 0);
  _err =
      cbor_encoder_create_array(&_encoderRoot, &_encoder, CborIndefiniteLength);
  assert(_err == 0);
  return *this;
}

CborSerializer &CborSerializer::end()
{
  if ( !_err)
  _err = cbor_encoder_close_container(&_encoderRoot, &_encoder);
  assert(_err == 0);
  if (_err)
    LOGW << "cbor error in end()" << _err << LEND;
  _size = cbor_encoder_get_buffer_size(&_encoderRoot, _buffer);
  return *this;
}

bool CborSerializer::success() { return _err == CborNoError; }

Bytes CborSerializer::toBytes() { return Bytes(_buffer, _buffer + _size); }
/*
CborSerializer& CborSerializer::addRaw(const Bytes& bs){
  _err = append_to_buffer(&_encoder, bs.data(), bs.size());
  return *this;
}
*/