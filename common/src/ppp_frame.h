#ifndef __FRAME_H_
#define __FRAME_H_
//#include <memory.h>
#include <stdint.h>

#include <log.h>
#include <broker_protocol.h>

#define PPP_FCS_SIZE 2
// PPP special characters
#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

Bytes ppp_frame(const Bytes &in);
bool ppp_deframe(Bytes &out, const Bytes &in);

#endif