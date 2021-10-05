#ifndef BEA3A05D_E4F4_41BB_A864_310EE1D37C62
#define BEA3A05D_E4F4_41BB_A864_310EE1D37C62

#include <CborDeserializer.h>
#include <CborSerializer.h>
#include <limero.h>

struct PubMsg {
  string topic;
  Bytes bs;
};

class BrokerAbstract {

 public:
  virtual int init() = 0;
  virtual int connect(string) = 0;
  virtual int disconnect() = 0;
  virtual int subscribe(string &);
  virtual int publish(string &, Bytes &) = 0;
  virtual int unSubscribe(string &) = 0;
  Source<PubMsg> &incoming();
  Source<bool> &connected();

  template <typename T>
  Sink<T> &publisher(TopicName topic) {
    SinkFunction<T> *sf = new SinkFunction<T>([&](const T &t) {
      static CborSerializer toCbor(100);
      Bytes bs = toCbor.begin().add(t).end().toBytes();
      publish(topic, bs);
    });
    return *sf;
  }

  template <typename T>
  Source<T> subscriber(string pattern) {
    auto lf = new LambdaFlow<PubMsg, T>([&](T &t, const PubMsg &msg) {
      CborDeserializer fromCbor(100);
      if (msg.topic != pattern) return false;
      return fromCbor.fromBytes(msg.bs).begin().get(t).success();
    });
    subscribe(pattern);
    incoming() >> lf;
    return *lf;
  }
};

#endif /* BEA3A05D_E4F4_41BB_A864_310EE1D37C62 */
