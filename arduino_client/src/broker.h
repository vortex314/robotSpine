#include <broker_protocol.h>
#include <logger.h>
#include <limero.h>
#include <ppp_frame.h>
#include <ReflectToCbor.h>
#include <ReflectFromCbor.h>
#include <CborDump.h>

namespace broker
{

  class Resource
  {
    int _id;
    TopicName _key;

  public:
    Resource(int i, TopicName &k) : _id(i), _key(k){};
    int id() { return _id; }
    const TopicName &key() { return _key; }
  };

  template <typename T>
  class Subscriber : public LambdaFlow<Bytes, T>, public Resource
  {
  public:
    Subscriber(int id, TopicName &key)
        : LambdaFlow<Bytes, T>(),
          Resource(id, key){};
  };

  template <typename T>
  class Publisher : public LambdaFlow<T, Bytes>, public Resource
  {
    CborSerializer _toCbor(100);
    TopicName topic;

  public:
    Publisher(int rid, TopicName &key)
        : LambdaFlow<T, Bytes>([&](Bytes &cb, const T &t)
                               {
                                 cb = _toCbor.begin().add(MsgPublish::TYPE).add(key).add(t).end().toBytes();
                                 return true;
                               }),
          Resource(rid, key), toCbor(*new ReflectToCbor(100)){};
  };

  class Broker : public Actor
  {
  public:
    ValueFlow<Bytes> toSerialMsg;
    ValueFlow<Bytes> fromSerialMsg;
    ValueFlow<Bytes> incomingPublish;
    ValueFlow<Bytes> incomingConnect;
    ValueFlow<Bytes> incomingDisconnect;

    int _resourceId = 0;

    std::vector<Resource *> _publishers;
    std::vector<Resource *> _subscribers;
    TopicName brokerSrcPrefix = "src/node/";
    TopicName brokerDstPrefix = "dst/node/";

  public:
    Broker(Thread thr) : Actor(thr){};
    template <typename T>
    Subscriber<T> &subscriber(TopicName);
    template <typename T>
    Publisher<T> &publisher(TopicName);
  };

  template <typename T>
  Publisher<T> &Broker::publisher(TopicName name)
  {
    TopicName topic = name;
    if (!(topic.startsWith("src/") || topic.startsWith("dst/")))
    {
      topic = brokerSrcPrefix + name;
    }
    Publisher<T> *p = new Publisher<T>(_resourceId++, topic);
    _publishers.push_back(p);
    *p >> toSerialMsg;
    LOGI << " created publisher : " << name.c_str() << LEND;
    return *p;
  }

  template <typename T>
  Subscriber<T> &Broker::subscriber(TopicName name)
  {
    TopicName topic = name;
    if (!(topic.startsWith("src/") || topic.startsWith("dst/")))
    {
      topic = brokerDstPrefix + name;
    }
    Subscriber<T> *s = new Subscriber<T>(_resourceId++, topic);
    s->lambda([&](T &t, const Bytes &in)
              {
                MsgPublish msgPublish;
                ReflectFromCbor fromCbor(100);
                if (msgPublish.reflect(fromCbor.fromBytes(in)).success())
                {
                  if (fromCbor.fromBytes(msgPublish.value).begin().member(t).end().success())
                    return true;
                }
                return false;
              });
    _subscribers.push_back(s);
    incomingPublish >> s;
    LOGI << " created subscriber :" << name.c_str() << LEND;
    return *s;
  }
}; // namespace broker
