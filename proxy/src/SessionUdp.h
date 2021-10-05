#ifndef _SESSION_SERAIL_H_
#define _SESSION_SERIAL_H_
#include <SessionAbstract.h>
#include <Udp.h>
#include <limero.h>

// typedef enum { CMD_OPEN, CMD_CLOSE } TcpCommand;

class UdpSessionError;

class SessionUdp : public SessionAbstract {
  UdpSessionError *_errorInvoker;
  int _serialfd;
  Udp _udp;
  int _port;
  string _interface;
  uint64_t _frameTimeout = 2000;
  QueueFlow<Bytes> _incomingMessage;
  QueueFlow<Bytes> _outgoingMessage;
  ValueFlow<Bytes> _logs;

  ValueFlow<bool> _connected;
  UdpMsg _udpMsg;

 public:
  //  ValueSource<TcpCommand> command;
  SessionUdp(Thread &thread, Config config);
  bool init();
  bool connect();
  bool disconnect();
  void onError();
  int fd();
  void invoke();
  Source<Bytes> &incoming();
  Sink<Bytes> &outgoing();
  Source<bool> &connected();
  Source<Bytes>& logs() ;
};

class UdpSessionError : public Invoker {
  SessionUdp &_udpSession;

 public:
  UdpSessionError(SessionUdp &udpSession) : _udpSession(udpSession){};
  void invoke() { _udpSession.onError(); }
};
#endif