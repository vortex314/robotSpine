#ifndef C4459101_F076_4820_A58D_E11FB9A9C0B8
#define C4459101_F076_4820_A58D_E11FB9A9C0B8
#include <context.h>
#include <cbor.h>
#include <etl/string_stream.h>
#include <log.h>

CborError dumpCborRecursive(etl::string_stream& ss, CborValue *it,
                                          int nestingLevel);
std::string cborDump(const Bytes &);

#endif /* C4459101_F076_4820_A58D_E11FB9A9C0B8 */
