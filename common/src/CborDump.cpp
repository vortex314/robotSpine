#include "CborDump.h"
#include <sstream>
std::string cborDump(const Bytes &bs) {
  std::stringstream ss;
  CborValue it;
  CborParser rootParser;
  cbor_parser_init(bs.data(), bs.size(), 0, &rootParser, &it);
  dumpCborRecursive(ss, &it, 0);
  return ss.str();
}
/**
 * Decode CBOR data manuallly
 */

#define CBOR_CHECK(a, str, goto_tag, ret_value, ...)                           \
  do {                                                                         \
    if ((a) != CborNoError) {                                                  \
      LOGW << "cbor error :" << str << LEND;                                   \
      ret = ret_value;                                                         \
      goto goto_tag;                                                           \
    }                                                                          \
  } while (0)

static void indent(std::stringstream &ss, int nestingLevel) {
  while (nestingLevel--) {
    ss << (" ");
  }
}
CborError dumpCborRecursive(stringstream &ss, CborValue *it, int nestingLevel) {
  CborError ret = CborNoError;
  bool first = true;
  while (!cbor_value_at_end(it)) {
    CborType type = cbor_value_get_type(it);

 //   indent(ss, nestingLevel);
    if (first)
      first = false;
    else
      ss << ",";
    switch (type) {
    case CborArrayType: {
      CborValue recursed;
      assert(cbor_value_is_container(it));
      ss << ("[");
      ret = cbor_value_enter_container(it, &recursed);
      CBOR_CHECK(ret, "enter container failed", err, ret);
      ret = dumpCborRecursive(ss, &recursed, nestingLevel + 1);
      CBOR_CHECK(ret, "recursive dump failed", err, ret);
      ret = cbor_value_leave_container(it, &recursed);
      CBOR_CHECK(ret, "leave container failed", err, ret);
      indent(ss, nestingLevel);
      ss << ("]");
      continue;
    }
    case CborMapType: {
      CborValue recursed;
      assert(cbor_value_is_container(it));
      ss << ("{");
      ret = cbor_value_enter_container(it, &recursed);
      CBOR_CHECK(ret, "enter container failed", err, ret);
      ret = dumpCborRecursive(ss, &recursed, nestingLevel + 1);
      CBOR_CHECK(ret, "recursive dump failed", err, ret);
      ret = cbor_value_leave_container(it, &recursed);
      CBOR_CHECK(ret, "leave container failed", err, ret);
      indent(ss, nestingLevel);
      ss << ("}");
      continue;
    }
    case CborIntegerType: {
      int64_t val;
      if (cbor_value_is_unsigned_integer(it)) {
        ret = cbor_value_get_int64_checked(it, &val); /* can't fail */
        CBOR_CHECK(ret, "parse int64 failed", err, ret);
        ss << (uint64_t)val;
      } else {
        /* CBOR stores the negative number X as -1 - X
         * (that is, -1 is stored as 0, -2 as 1 and so forth) */
        if (++val) { /* unsigned overflow may happen */
          ss << (int64_t)val;
        } else {
          ss << -1234567890123456LL;
        }
      }
      break;
    }
    case CborByteStringType: {
      uint8_t *buf;
      size_t n;
      ret = cbor_value_dup_byte_string(it, &buf, &n, it);
      CBOR_CHECK(ret, "parse byte string failed", err, ret);
      ss << "h'" << hexDump(Bytes(buf, buf + n)).c_str();
      ss << ("'");
      free(buf);
      continue;
    }
    case CborTextStringType: {
      char *buf;
      size_t n;
      ret = cbor_value_dup_text_string(it, &buf, &n, it);
      CBOR_CHECK(ret, "parse text string failed", err, ret);
      ss << '"' << (const char *)buf << '"';
      free(buf);
      //    ret = cbor_value_advance(it);
      continue;
    }
    case CborTagType: {
      CborTag tag;
      ret = cbor_value_get_tag(it, &tag);
      CBOR_CHECK(ret, "parse tag failed", err, ret);
      ss << "Tag(" << (long long)tag << ")";
      break;
    }
    case CborSimpleType: {
      uint8_t type;
      ret = cbor_value_get_simple_type(it, &type);
      CBOR_CHECK(ret, "parse simple type failed", err, ret);
      ss << "simple(" << type << ")";
      break;
    }
    case CborNullType:
      ss << ("null");
      break;
    case CborUndefinedType:
      ss << ("undefined");
      break;
    case CborBooleanType: {
      bool val;
      ret = cbor_value_get_boolean(it, &val);
      CBOR_CHECK(ret, "parse boolean type failed", err, ret);
      ss << (val ? "true" : "false");
      break;
    }
    case CborHalfFloatType: {
      uint16_t val;
      ret = cbor_value_get_half_float(it, &val);
      CBOR_CHECK(ret, "parse half float type failed", err, ret);
      ss << "__f16(" << std::hex << val << ")";
      break;
    }
    case CborFloatType: {
      float val;
      ret = cbor_value_get_float(it, &val);
      CBOR_CHECK(ret, "parse float type failed", err, ret);
      ss << val;
      break;
    }
    case CborDoubleType: {
      double d;
      float f;
      if (cbor_value_is_double(it))
        ret = cbor_value_get_double(it, &d);
      else if (cbor_value_is_float(it)) {
        ret = cbor_value_get_float(it, &f);
        d = f;
      }
      CBOR_CHECK(ret, "parse double float type failed", err, ret);
      ss << std::setprecision(6) << d << ",";
      break;
    }
    case CborInvalidType: {
      ret = CborErrorUnknownType;
      CBOR_CHECK(ret, "unknown cbor type", err, ret);
      ss << "#";
      break;
    }
    default: {
      WARN(" invalid cbor type ");
    }
    }

    ret = cbor_value_advance(it);
    CBOR_CHECK(ret, "fix value failed", err, ret);
  }
  return CborNoError;
err:
  return ret;
}