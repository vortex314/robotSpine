#include <cbor.h>
#include <malloc.h>
#include <stdint.h>
#include <vector>

/*
9F                                      # array(*)
   9F                                   # array(*)
      00                                # unsigned(0)
      77                                # text(23)
         7372632F73746D3332663130332F73797374656D2F7069 # "src/stm32f103/system/pi"
      FB 400921FB542FE938               # primitive(4614256656550717752)
      FF                                # primitive(*)
   FF                                   # primitive(*)
*/

int main(int argc, char **argv) {
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
}