#ifndef _SESSION_SERAIL_H_
#define _SESSION_SERIAL_H_
#include <Frame.h>
#include <SessionAbstract.h>
#include <limero.h>
#include <serial.h>

// typedef enum { CMD_OPEN, CMD_CLOSE } TcpCommand;

class SerialSessionError;

class SessionSerial : public SessionAbstract {
  SerialSessionError *_errorInvoker;
  int _serialfd;
  Serial _serialPort;
  string _port;
  uint32_t _baudrate;
  Bytes _rxdBuffer;
  Bytes _inputFrame;
  Bytes _cleanData;
  uint64_t _lastFrameFlag;
  uint64_t _frameTimeout = 2000;
  BytesToFrame bytesToFrame;
  FrameToBytes frameToBytes;
  QueueFlow<Bytes> _incomingSerialRaw;
  QueueFlow<Bytes> _incomingFrame;
  // QueueFlow<String> _incomingMessage;
  QueueFlow<Bytes> _outgoingFrame;
  ValueFlow<bool> _connected;
  ValueFlow<String> _logs;

public:
  //  ValueSource<TcpCommand> command;
  SessionSerial(Thread &thread, Config config);
  bool init();
  bool connect();
  bool disconnect();
  void onError();
  int fd();
  void invoke();
  Source<Bytes> &incoming();
  Sink<Bytes> &outgoing();
  Source<bool> &connected();
  Source<String> &logs();
  string port();
};

class SerialSessionError : public Invoker {
  SessionSerial &_serialSession;

public:
  SerialSessionError(SessionSerial &serialSession)
      : _serialSession(serialSession){};
  void invoke() { _serialSession.onError(); }
};
#endif