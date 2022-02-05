#include <Sys.h>
#include <cbor.h>
#include <Log.h>
#include <malloc.h>
#include <stdint.h>

#include <vector>

#define BUFFER_SIZE 100
#define START 18446744071562584364UL

void parse(uint8_t *data, size_t size,uint64_t val) {
  CborParser rootParser;
  CborValue mainIt;
  CborValue arrayIt;
  CborError rc;
  int arg1;
  uint64_t arg2;
  char *arg3;
  size_t sz;
  rc = cbor_parser_init(data, size, 0, &rootParser, &mainIt);
  assert(rc == CborNoError);
  rc = cbor_value_enter_container(&mainIt, &arrayIt);
  assert(rc == CborNoError);
  rc = cbor_value_get_int(&arrayIt, &arg1);
  assert(rc == CborNoError);
  rc = cbor_value_advance(&arrayIt);
  assert(rc == CborNoError);
  rc = cbor_value_dup_text_string(&arrayIt, &arg3, &sz, 0);
  arg3[sz]=0;
  assert(rc == CborNoError);
//  printf(" %s ",arg3);
  ::free(arg3);
  rc = cbor_value_advance(&arrayIt);
  assert(rc == CborNoError);
  rc = cbor_value_get_uint64(&arrayIt, &arg2);
  assert(arg2==val);
 // printf("%lu \n", arg2);
  rc = cbor_value_advance(&arrayIt);
  assert(rc == CborNoError);
  rc = cbor_value_leave_container(&mainIt, &arrayIt);
  assert(rc == CborNoError);
}

int main(int argc, char **argv) {
  CborEncoder *rootEncoder = new CborEncoder;
  CborEncoder *arrayEncoder = new CborEncoder;
  int arg1 = 123;
  std::string arg2 = "dst/ESP32-45083/system/loopback";
  uint64_t arg3;
  uint8_t data[BUFFER_SIZE];
  for (arg3 = START; arg3 < (START + 1000000); arg3++) {
    cbor_encoder_init(rootEncoder, data, BUFFER_SIZE, 0);
    CborError rc = cbor_encoder_create_array(rootEncoder, arrayEncoder,
                                             CborIndefiniteLength);
    assert(rc == CborNoError);
    rc = cbor_encode_int(arrayEncoder, arg1);
    assert(rc == CborNoError);
    rc = cbor_encode_text_string(arrayEncoder, arg2.c_str(), arg2.size());
    assert(rc == CborNoError);
    rc = cbor_encode_uint(arrayEncoder, arg3);
    assert(rc == CborNoError);
    rc = cbor_encoder_close_container(rootEncoder, arrayEncoder);
    assert(rc == CborNoError);
    size_t size = cbor_encoder_get_buffer_size(rootEncoder, data);
    parse(data, size,arg3);
 //   printf("%s => ", hexDump(Bytes(data, data + size)).c_str());
  }
  delete arrayEncoder;
  delete rootEncoder;
}
/*
9F 00 78 1D 64 73 74 2F 73 74 6D 33 32 66 31 30 33 2F 73 79 73 74 65 6D 2F 6C 6F
6F 70 62 61 63 6B 18 C8 FF
*/
/*
9F                                      # array(*)
   9F                                   # array(*)
      00                                # unsigned(0)
      77                                # text(23)
         7372632F73746D3332663130332F73797374656D2F7069 #
"src/stm32f103/system/pi" FB 400921FB542FE938               #
primitive(4614256656550717752) FF                                # primitive(*)
   FF                                   # primitive(*)
*/
Log logger;
int main2(int argc, char **argv) {
  std::vector<uint8_t> bytes = {
      0x9F, 0x00, 0x77, 0x73, 0x72, 0x63, 0x2F, 0x73, 0x74, 0x6D, 0x33, 0x32,
      0x66, 0x31, 0x30, 0x33, 0x2F, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x2F,
      0x70, 0x69, 0xFB, 0x40, 0x09, 0x21, 0xFB, 0x54, 0x2F, 0xE9, 0x38, 0xFF};
  CborParser rootParser;
  CborValue mainIt;
  CborValue arrayIt;
  int arg1;
  char *arg2;
  double arg3;
  cbor_parser_init(bytes.data(), bytes.size(), 0, &rootParser, &mainIt);
  cbor_value_enter_container(&mainIt, &arrayIt);
  cbor_value_get_int(&arrayIt, &arg1);
  cbor_value_advance(&arrayIt);
  size_t n;
  cbor_value_dup_text_string(&arrayIt, &arg2, &n, &arrayIt);
  free(arg2);
  cbor_value_get_double(&arrayIt, &arg3);
  cbor_value_advance(&arrayIt);
  cbor_value_leave_container(&mainIt, &arrayIt);
  return 0;
}