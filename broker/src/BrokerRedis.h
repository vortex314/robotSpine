#ifndef _BROKER_REDIS_H_
#define _BROKER_DREDIS_H_
//#include <async.h>
#include <hiredis.h>
#include <log.h>

#include "BrokerBase.h"
#include "limero.h"
#include <set>

using namespace std;

struct SubscriberStruct {
  string pattern;
};

class BrokerRedis : public BrokerBase {
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

 public:
  ValueFlow<bool> connected;
  ValueFlow<unordered_map<string,string>> stream;

  BrokerRedis(Thread &, Config );
  int init();
  int connect(string);
  int disconnect();
  int reconnect();
  int publish(string, const Bytes &);
  int onSubscribe(SubscribeCallback);
  int unSubscribe(string);
  int subscribe(string);
  int subscribeAll();
  bool match(string pattern, string source);
  redisReply* xread(string key);
  int command(const char *format, ...);
  int request(string cmd,std::function<void(redisReply* )> func);
  int getId(string);
  int newRedisPublisher(string topic);
  vector<PubMsg> query(string);
  string replyToString(void* r);
};

// namespace zenoh
#endif  // _ZENOH_SESSION_h_