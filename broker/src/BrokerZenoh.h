
#ifndef _BROKER_ZENOH_H_
#define _BROKER_ZENOH_H_
#include "BrokerBase.h"
#include "limero.h"
#include <log.h>
extern "C" {
#include "zenoh.h"
#include "zenoh/net.h"
}
#define US_WEST "tcp/us-west.zenoh.io:7447"
#define US_EAST "tcp/us-east.zenoh.io:7447"

using namespace std;

struct SubscriberStruct {
  string pattern;
  std::function<void(string &, const Bytes &)> callback;
  zn_subscriber_t *zn_subscriber;
};

struct PublisherStruct {
  string key;
  zn_reskey_t zn_reskey;
  zn_publisher_t *zn_publisher;
};

class BrokerZenoh : public BrokerBase {
  zn_session_t *_zenoh_session;
  unordered_map<string, SubscriberStruct *> _subscribers;
  unordered_map<string, PublisherStruct *> _publishers;
  static void subscribeHandler(const zn_sample_t *, const void *);
  zn_reskey_t resource(string topic);
  int scout();
  ValueFlow<bool> _connected;
  int newZenohPublisher(string topic);

 public:
  BrokerZenoh(Thread &, Config &);
  int init();
  int connect(string);
  int disconnect();
  int publish(string , const Bytes &);
  int subscribe(string );
  int unSubscribe(string);
  bool match(string pattern, string source);
  vector<PubMsg> query(string);
};

// namespace zenoh
#endif // _ZENOH_SESSION_h_