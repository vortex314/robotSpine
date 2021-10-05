#if defined( __ESP8266_RTOS__ )|| defined(__LM4F120_ARDUINO__)
// This is needed to get compiled or Arduino
#include <stdint.h>
#include <cstring>
extern "C" void _exit(){};
bool isnan(double x) 
{
    int64_t hx;

    memcpy(&hx, &x, sizeof(hx));
    hx &= UINT64_C(0x7fffffffffffffff);
    hx = UINT64_C(0x7ff0000000000000) - hx;
    return (int)(((uint64_t)hx) >> 63);
}
bool isinf(double x)
{
    int64_t ix;

    memcpy(&ix, &x, sizeof(ix));
    int64_t t = ix & UINT64_C(0x7fffffffffffffff);
    t ^= UINT64_C(0x7ff0000000000000);
    t |= -t;
    return ~(t >> 63) & (ix >> 62);
}
bool isnan(float x)
{
    int32_t ix;
    memcpy(&ix, &x, sizeof(ix));
    ix &= 0x7fffffff;
    ix = 0x7f800000 - ix;
    return (int)(((uint32_t)(ix)) >> 31);
}
bool isinf(float x)
{
    int32_t ix, t;
    memcpy(&ix, &x, sizeof(ix));
    t = ix & 0x7fffffff;
    t ^= 0x7f800000;
    t |= -t;
    return ~(t >> 31) & (ix >> 30);
}
#endif