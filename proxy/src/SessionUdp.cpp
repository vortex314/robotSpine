#include <SessionUdp.h>
#include <ppp_frame.h>

SessionUdp::SessionUdp(Thread &thread, Config config)
    : SessionAbstract(thread, config),
      _incomingMessage(10, "_incomingMessage"),
      _outgoingMessage(10, "_outgoingMessage") {
  _errorInvoker = new UdpSessionError(*this);
  _port = config["port"].as<int>();
}

bool SessionUdp::init() {
  _udp.port(_port);
  _udp.init();

  return true;
}

bool SessionUdp::connect() {
  thread().addReadInvoker(_udp.fd(), [&](int) { invoke(); });
  thread().addErrorInvoker(_udp.fd(), [&](int) { invoke(); });
  return true;
}

bool SessionUdp::disconnect() {
  thread().deleteInvoker(_udp.fd());
  _udp.deInit();
  return true;
}
// on data incoming on file descriptor
void SessionUdp::invoke() {
  int rc = _udp.receive(_udpMsg);
  if (rc == 0) {  // read ok
    _incomingMessage.on(_udpMsg.message);
  }
}
// on error issue onf ile descriptor
void SessionUdp::onError() {
  LOGW << " Error occured on SessionUdp. Disconnecting.. " << LEND;
  disconnect();
}

int SessionUdp::fd() { return _udp.fd(); }

Source<Bytes> &SessionUdp::incoming() { return _incomingMessage; }

Sink<Bytes> &SessionUdp::outgoing() { return _outgoingMessage; }

Source<bool> &SessionUdp::connected() { return _connected; }
Source<Bytes> &SessionUdp::logs() { return _logs; }