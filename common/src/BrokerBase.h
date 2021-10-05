#ifndef BEA3A05D_E4F4_41BB_A864_310EE1D37C62
#define BEA3A05D_E4F4_41BB_A864_310EE1D37C62

#include <CborDeserializer.h>
#include <CborSerializer.h>
#include <limero.h>
typedef int (*SubscribeCallback)(int, Bytes);

struct PubMsg
{
  String topic;
  Bytes payload;
};

struct SubMsg
{
  String pattern;
};

struct NodeMsg
{
  String node;
};

class BrokerBase : public Actor
{
  QueueFlow<PubMsg> _incoming;
  QueueFlow<PubMsg> _outgoing;
  CborSerializer _toCbor;
  CborDeserializer _fromCbor;
  std::string _srcPrefix;
  std::string _dstPrefix;
  friend class BrokerZenoh;
  friend class BrokerRedis;
  friend class BrokerSerial;

public:
  BrokerBase(Thread &thr, Config &)
      : Actor(thr), _incoming(10, "incoming"), _outgoing(10, "outgoing"),
        _toCbor(1000000), _fromCbor(1000000){};
  virtual int init() = 0;
  virtual int connect(String) = 0;
  virtual int disconnect() = 0;
  virtual int publish(String, const Bytes &) = 0;
  virtual int subscribe(String) = 0;
  virtual int unSubscribe(String) = 0;
  virtual bool match(String pattern, String source) = 0;

  ValueFlow<bool> connected;
  Source<PubMsg> &incoming() { return _incoming; };

  template <typename T>
  Sink<T> &publisher(TopicName topic)
  {
    std::string absTopic = _srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t)
                                              {
                                                Bytes bs = _toCbor.begin().add(t).end().toBytes();
                                                _outgoing.on({absTopic, bs});
                                              });
    return *sf;
  }
  template <typename T>
  Source<T> &subscriber(std::string pattern)
  {
    std::string absPattern = _dstPrefix + pattern;
    if (pattern.rfind("src/", 0) == 0 || pattern.rfind("dst/", 0) == 0)
      absPattern = pattern;
    auto lf = new LambdaFlow<PubMsg, T>([&, absPattern](T &t, const PubMsg &msg)
                                        {
                                          if (msg.topic == absPattern || match(absPattern, msg.topic))
                                            return _fromCbor.fromBytes(msg.payload).begin().get(t).success();
                                          else
                                            return false;
                                        });
    _incoming >> lf;
    return *lf;
  }
};

#endif /* BEA3A05D_E4F4_41BB_A864_310EE1D37C62 */
