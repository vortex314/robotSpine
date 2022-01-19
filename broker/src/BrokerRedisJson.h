#ifndef _BROKER_REDIS_H_
#define _BROKER_DREDIS_H_
//#include <async.h>
#include <hiredis.h>
#include <Log.h>
#include <broker_protocol.h>

#include "limero.h"
#include <set>
#include <nlohmann/json.hpp>
using Json = nlohmann::json;

typedef int (*SubscribeCallback)(int, string);

struct PubMsg
{
  String topic;
  Json payload;
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
  static string replyToString(void *r);


  template <typename T>
  Sink<T> &publisher(String topic) {
    String absTopic = _srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t) {
      Json json =  t;
      _outgoing.on({absTopic,json});
    });
    return *sf;
  }

  template <typename T>
  Source<T> &subscriber(String pattern) {
    String absPattern = _dstPrefix + pattern;
    if (pattern.rfind("src/", 0) == 0 || pattern.rfind("dst/", 0) == 0)
      absPattern = pattern;
    auto lf = new LambdaFlow<PubMsg, T>([&, absPattern](T &t, const PubMsg &msg) {
//      INFO(" %s vs %s ",msg.topic.c_str(),absPattern.c_str());
      if (msg.topic == absPattern /*|| match(absPattern, msg.topic)*/) {
        t = msg.payload.get<T>();
        return true;
      }
      return false;
    });
    _incoming >> lf;
    return *lf;
  }
  
};

// namespace zenoh
#endif // _ZENOH_SESSION_h_