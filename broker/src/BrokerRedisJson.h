#ifndef _BROKER_REDIS_H_
#define _BROKER_DREDIS_H_
//#include <async.h>
#include <hiredis.h>
#include <log.h>

#include "limero.h"
#include <set>
#include <ArduinoJson.h>

typedef int (*SubscribeCallback)(int, string);

struct PubMsg
{
  std::string topic;
  std::string payload;
};

struct SubscriberStruct
{
  string pattern;
};

class BrokerRedis
{
  Thread &_thread;
  set<string> _subscriptions;
  int scout();
  string _hostname;
  uint16_t _port;
  string _node;
  redisContext *_subscribeContext;
  redisContext *_publishContext;
  Thread *_publishEventThread;
  Thread *_subscribeEventThread;
  struct event_base *_publishEventBase;
  struct event_base *_subscribeEventBase;
  TimerSource _reconnectTimer;
  static void onMessage(redisContext *c, void *reply, void *me);
  ValueFlow<bool> _reconnectHandler;
  std::string _srcPrefix;
  std::string _dstPrefix;
  QueueFlow<PubMsg> _incoming;
  QueueFlow<PubMsg> _outgoing;

public:
  ValueFlow<bool> connected;
  ValueFlow<unordered_map<string, string>> stream;
  Source<PubMsg> &incoming() { return _incoming; };

  BrokerRedis(Thread &, Config);
  ~BrokerRedis();
  int init();
  int connect(string);
  int disconnect();
  int reconnect();
  int publish(string, const string &);
  int onSubscribe(SubscribeCallback);
  int unSubscribe(string);
  int subscribe(string);
  int subscribeAll();
  bool match(string pattern, string source);
  redisReply *xread(string key);
  int command(const char *format, ...);
  int request(string cmd, std::function<void(redisReply *)> func);
  int getId(string);
  int newRedisPublisher(string topic);
  vector<PubMsg> query(string);
  string replyToString(void *r);

  template <typename T>
  Sink<T> &publisher(TopicName topic)
  {
    std::string absTopic = _srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t)
                                              {
                                                DynamicJsonDocument doc(1024);
                                                JsonVariant variant = doc.to<JsonVariant>();
                                                variant.set(t);
                                                std::string result;
                                                serializeJson(doc, result);
                                                _outgoing.on({absTopic, result});
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
                                          {
                                            DynamicJsonDocument doc(1024);
                                            DeserializationError rc = deserializeJson(doc, msg.payload);
                                            if (rc == DeserializationError::Ok)
                                            {
                                              t = doc.as<T>();
                                              return true;
                                            }
                                          }
                                          return false;
                                        });
    _incoming >> lf;
    return *lf;
  }
};

// namespace zenoh
#endif // _ZENOH_SESSION_h_