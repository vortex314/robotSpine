
#include "BrokerSerial.h"

//================================================================
class MsgFilter : public LambdaFlow<Bytes, Bytes>
{
  int _msgType;
  CborDeserializer _fromCbor;

public:
  MsgFilter(int msgType)
      : LambdaFlow<Bytes, Bytes>([&](Bytes &out, const Bytes &in)
                                 {
                                   int msgType;
                                   if (_fromCbor.fromBytes(in).begin().get(msgType).success() &&
                                       msgType == _msgType)
                                   {
                                     out = in;
                                     return true;
                                   }
                                   return false;
                                 }),
        _fromCbor(100)
  {
    _msgType = msgType;
  };
  static MsgFilter &nw(int msgType) { return *new MsgFilter(msgType); }
};

BrokerSerial::BrokerSerial(Thread &thr, Stream &serial)
    : Actor(thr),
      _serial(serial),
      _toCbor(100), _fromCbor(100),
      _loopbackTimer(thr, 1000, true, "loopbackTimer"),
      _connectTimer(thr, 3000, true, "connectTimer"),
      _outgoing(10, "_outgoing"),
      _incoming(5, "_incoming")
{
  node(Sys::hostname());
  _outgoing.async(thr); // reduces stack need
  _incoming.async(thr);
  connected.async(thr);
}

BrokerSerial::~BrokerSerial() {}

void BrokerSerial::node(const char *n)
{
  _node = n;
  _srcPrefix = "src/" + _node + "/";
  _dstPrefix = "dst/" + _node + "/";
  _loopbackTopic = _dstPrefix + "system/loopback";
  connected = false;
};

int BrokerSerial::init()
{
  // outgoing
  _uptimePublisher = &publisher<uint64_t>("system/uptime");
  _latencyPublisher = &publisher<uint64_t>("system/latency");
  _loopbackSubscriber = &subscriber<uint64_t>("system/loopback");
  _loopbackPublisher = &publisher<uint64_t>(_loopbackTopic);

  /* _fromSerialFrame >> [&](const Bytes &bs)
  { LOGI << "RXD [" << bs.size() << "] " << cborDump(bs).c_str() << LEND; };
  _toSerialFrame >> [&](const Bytes &bs)
  { LOGI << "TXD [" << bs.size() << "] " << cborDump(bs).c_str() << LEND; };
*/
  _toSerialFrame >> _frameToBytes >> [&](const Bytes &bs)
  {
    _serial.write(bs.data(), bs.size());
  };

  _outgoing >> [&](const PubMsg &msg)
  {
    if (_toCbor.begin().add(B_PUBLISH).add(msg.topic).add(msg.payload).end().success())
      _toSerialFrame.on(_toCbor.toBytes());
  };

  //serialRxd >> [](const Bytes &bs)
  //{ INFO("RXD %d", bs.size()); };
  serialRxd >> _fromSerialFrame;

  _fromSerialFrame >> [&](const Bytes &msg)
  {
    int msgType;
    PubMsg pubMsg;
    if (_fromCbor.fromBytes(msg).begin().get(msgType).get(pubMsg.topic).get(pubMsg.payload).end().success() && msgType == B_PUBLISH)
    {
      _incoming.on(pubMsg);
    }
    else
      WARN(" no publish Msg %s", cborDump(msg).c_str());
  };

  _loopbackTimer >> [&](const TimerMsg &tm)
  {
    if (!connected())
      sendNode(Sys::hostname());
    _loopbackPublisher->on(Sys::millis());
  };

  subscriber<uint64_t>(_loopbackTopic) >>
      [&](const uint64_t &in)
  {
    _latencyPublisher->on(Sys::millis() - in);
    connected = true;
    _connectTimer.reset();
  };

  _connectTimer >> [&](const TimerMsg &)
  { connected = false; };

  _serial.setTimeout(0);
  return 0;
}

void BrokerSerial::onRxd(void *me)
{
  BrokerSerial *brk = (BrokerSerial *)me;
  Bytes data;
  while (brk->_serial.available())
  {
    data.push_back(brk->_serial.read());
  }
  brk->serialRxd.emit(data);
}

int BrokerSerial::publish(std::string topic, Bytes &bs)
{
  _outgoing.on({topic, bs});
  return 0;
}

int BrokerSerial::subscribe(std::string pattern)
{
  if (_toCbor.begin().add(B_SUBSCRIBE).add(pattern).end().success())
    _toSerialFrame.on(_toCbor.toBytes());
    return 0;
}

int BrokerSerial::sendNode(std::string nodeName){
  if (_toCbor.begin().add(B_NODE).add(nodeName).end().success())
    _toSerialFrame.on(_toCbor.toBytes());
    return 0;
}
