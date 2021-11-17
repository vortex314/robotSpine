#include <CborDump.h>
#include <SessionSerial.h>
#include <ppp_frame.h>

class BytesToString : public LambdaFlow<Bytes, String> {
public:
  BytesToString()
      : LambdaFlow<Bytes, String>([&](String &out, const Bytes &in) {
          out = String((const char *)in.data(),
                       (const char *)(in.data() + in.size()));
          return true;
        }){};
};

class StringToBytes : public LambdaFlow<String, Bytes> {
public:
  StringToBytes()
      : LambdaFlow<String, Bytes>([&](Bytes &out, const String &in) {
          out = Bytes(in.data(), in.data() + in.length());
          return true;
        }){};
};

SessionSerial::SessionSerial(Thread &thread, Config config)
    : SessionAbstract(thread, config), _incomingFrame(10, "_incomingMessage"),
      _outgoingFrame(10, "_outgoingMessage"),
      _incomingSerialRaw(10, "_incomingSerial") {
  _errorInvoker = new SerialSessionError(*this);
  _port = config["port"].get<String>();
  _baudrate = config["baudrate"].get<uint32_t>();
  _incomingSerialRaw.async(thread);
  _outgoingFrame.async(thread);
  _incomingFrame.async(thread);
}

bool SessionSerial::init() {
  _serialPort.port(_port);
  _serialPort.baudrate(_baudrate);
  _serialPort.init();
  _incomingSerialRaw >> bytesToFrame >> _incomingFrame;
  bytesToFrame.logs >> new BytesToString() >> _logs;
  _outgoingFrame >> frameToBytes >> [&](const Bytes &data) {
    INFO("TXD  %s => [%d] %s", _serialPort.port().c_str(),data.size(), hexDump(data).c_str());
    _serialPort.txd(data);
  };
  _outgoingFrame >> [&](const Bytes &bs) {
    INFO("TXD [%d] %s ",bs.size(), hexDump(bs).c_str());
    INFO("TXD %s ", cborDump(bs).c_str());
  };

  bytesToFrame >> [&](const Bytes &data) {
    //      INFO("RXD frame  %s", hexDump(data).c_str());
  };
  _incomingSerialRaw >> [&](const Bytes &data) {
    //   INFO("RXD raw %d", data.size());
  };
  return true;
}

bool SessionSerial::connect() {
  _serialPort.connect();
  thread().addReadInvoker(_serialPort.fd(), [&](int) { invoke(); });
  thread().addErrorInvoker(_serialPort.fd(), [&](int) { onError(); });
  return true;
}

bool SessionSerial::disconnect() {
  thread().deleteInvoker(_serialPort.fd());
  _serialPort.disconnect();
  return true;
}
// on data incoming on filedescriptor
void SessionSerial::invoke() {
  int rc = _serialPort.rxd(_rxdBuffer);
  if (rc == 0) {                  // read ok
    if (_rxdBuffer.size() == 0) { // but no data
      WARN(" 0 data ");
    } else {
      _incomingSerialRaw.on(_rxdBuffer);
    }
  }
}
// on error issue onf ile descriptor
void SessionSerial::onError() {
  INFO(" error occured on serial ,disconnecting ");
  disconnect();
}

int SessionSerial::fd() { return _serialPort.fd(); }

Source<Bytes> &SessionSerial::incoming() { return _incomingFrame; }

Sink<Bytes> &SessionSerial::outgoing() { return _outgoingFrame; }

Source<bool> &SessionSerial::connected() { return _connected; }

Source<String> &SessionSerial::logs() { return _logs; }

string SessionSerial::port() { return _port; }