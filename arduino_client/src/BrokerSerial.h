#ifndef BrokerSerial_H
#define BrokerSerial_H
#include <log.h>
#include <ReflectFromCbor.h>
#include <ReflectToCbor.h>
#include <broker_protocol.h>
#include <limero.h>
#include <Frame.h>
#include <CborDump.h>

struct PubMsg
{
  std::string topic;
  Bytes payload;
};

class BrokerSerial : Actor
{
  Stream &_serial;
  ValueFlow<Bytes> serialRxd;
  Sink<uint64_t> *_uptimePublisher;
  Sink<uint64_t> *_latencyPublisher;
  Sink<uint64_t> *_loopbackPublisher;
  Source<uint64_t> *_loopbackSubscriber;
  ValueFlow<Bytes> _toSerialFrame;

  BytesToFrame _fromSerialFrame;
  FrameToBytes _frameToBytes;
  CborSerializer _toCbor;
  CborDeserializer _fromCbor;

  std::string _node;
  std::string _dstPrefix;
  std::string _srcPrefix;
  std::string _loopbackTopic;
  TimerSource _loopbackTimer;
  TimerSource _connectTimer;
  QueueFlow<PubMsg> _outgoing;
  QueueFlow<PubMsg> _incoming;

public:
  ValueFlow<bool> connected;
  static void onRxd(void *);
  BrokerSerial(Thread &thr, Stream &serial);
  ~BrokerSerial();
  void node(const char *);
  int init();
  int publish(std::string, Bytes &);
  int subscribe(std::string);
  int sendNode(std::string);

  template <typename T>
  Sink<T> &publisher(std::string topic)
  {
    std::string absTopic = _srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t)
                                              {
                                                if (_toCbor.begin().add(t).end().success())
                                                {
                                                  _outgoing.on({absTopic, _toCbor.toBytes()});
                                                }
                                              });
    return *sf;
  }
  template <typename T>
  Source<T> &subscriber(std::string topic)
  {
    std::string absTopic = _dstPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    auto lf = new LambdaFlow<PubMsg, T>([&, absTopic](T &t, const PubMsg &msg)
                                        {
                                          if (absTopic == msg.topic)
                                            return _fromCbor.fromBytes(msg.payload).begin().get(t).success();
                                          else
                                            return false;
                                        });
    _incoming >> lf;
    return *lf;
  }
};

#endif // BrokerSerial_H
