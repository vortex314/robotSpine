#ifndef A4D87CFA_B381_4A7A_9FB2_67EDFB2285CB
#define A4D87CFA_B381_4A7A_9FB2_67EDFB2285CB
#include <limero.h>
#include <frame.h>
//#include <broker_protocol.h>
//================================================================
class BytesToFrame : public Flow<Bytes, Bytes>
{
  Bytes _inputFrame;
  Bytes _cleanData;
  uint64_t _lastFrameFlag;
  uint32_t _frameTimeout = 1000;

public:
  ValueFlow<Bytes> logs;
  BytesToFrame();
  void on(const Bytes &bs);
  void toStdout(const Bytes &bs);
  bool handleFrame(const Bytes &bs);
  void handleRxd(const Bytes &bs);
  void request();
};
class FrameToBytes : public LambdaFlow<Bytes, Bytes>
{
public:
  FrameToBytes();
};

#endif /* A4D87CFA_B381_4A7A_9FB2_67EDFB2285CB */
