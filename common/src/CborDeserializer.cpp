#include "CborDeserializer.h"

#undef assert
#define assert(expr)                                                           \
  if (!(expr))                                                                 \
    LOGW << " assert failed :" #expr << LEND;

CborDeserializer::CborDeserializer(size_t size) {
  _buffer = new uint8_t[size];
  _capacity = size;
}
CborDeserializer::~CborDeserializer() { delete[] _buffer; }

CborDeserializer &CborDeserializer::begin() {
  _err = CborNoError;

  _err = cbor_parser_init(_buffer, _size, 0, &_decoder, &_rootIt);
  assert(_err == CborNoError);
  if (cbor_value_is_container(&_rootIt)) {
    _err = cbor_value_enter_container(&_rootIt, &_it);
    assert(_err == CborNoError);
  } else {
    _err = CborErrorIllegalType;
  }
  return *this;
};

CborDeserializer &CborDeserializer::end() {
  //  _err = cbor_value_leave_container(&_rootIt, &_it); // don't leave
  //  container , not sure you reached the end of it assert(_err ==
  //  CborNoError);
  return *this;
};

CborDeserializer &CborDeserializer::get(Bytes &t) {
  size_t size;
  if (!_err && cbor_value_is_byte_string(&_it) &&
      cbor_value_calculate_string_length(&_it, &size) == 0) {
    uint8_t* temp;
    size_t size;
    _err = cbor_value_dup_byte_string(&_it, &temp, &size, 0);
    assert(_err == CborNoError);
    t = Bytes(temp, temp + size);
    free(temp);
    if (!_err)
      _err = cbor_value_advance(&_it);
  } else {
    _err = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(std::string &t) {
  if (!_err && cbor_value_is_text_string(&_it)) {
    char *temp;
    size_t size;
    _err = cbor_value_dup_text_string(&_it, &temp, &size, 0);
    assert(_err == CborNoError);
    t = temp;
    ::free(temp);
    if (!_err)
      _err = cbor_value_advance(&_it);
  }else {
    _err = CborErrorIllegalType;
  }
  return *this;
}

CborDeserializer &CborDeserializer::get(int &t) {
  if (!_err && cbor_value_is_integer(&_it)) {
    _err = cbor_value_get_int(&_it, &t);
    if (!_err)
      _err = cbor_value_advance_fixed(&_it);
  } else {
    _err = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(int64_t &t) {
  if (!_err && cbor_value_is_integer(&_it)) {
    _err = cbor_value_get_int64(&_it, &t);
    if (!_err)
      _err = cbor_value_advance(&_it);
  } else {
    _err = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(uint64_t &t) {
  if (!_err && cbor_value_is_unsigned_integer(&_it)) {
    _err = cbor_value_get_uint64(&_it, &t);
    if (!_err)
      _err = cbor_value_advance_fixed(&_it);
  } else {
    _err = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(double &t) {
  if (!_err && cbor_value_is_double(&_it)) {
    _err = cbor_value_get_double(&_it, &t);
    if (!_err)
      _err = cbor_value_advance(&_it);
  } else {
    _err = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(const int &t) {
  int x;
  if (!_err && cbor_value_is_integer(&_it)) {
    _err = cbor_value_get_int(&_it, &x);
    assert(_err == CborNoError);
    if (!_err)
      _err = cbor_value_advance_fixed(&_it);
  } else {
    _err = CborErrorIllegalType;
  }
  return *this;
}

CborDeserializer &CborDeserializer::fromBytes(const Bytes &bs) {
  assert(bs.size() < _capacity);
  _size = bs.size() < _capacity ? bs.size() : _capacity;
  memcpy(_buffer, bs.data(), _size);
  return *this;
}
bool CborDeserializer::success() {
 
  return _err == 0;
}
