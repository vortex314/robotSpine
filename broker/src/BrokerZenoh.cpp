
#include <BrokerZenoh.h>
#include <CborDump.h>

void BrokerZenoh::subscribeHandler(const zn_sample_t *sample, const void *arg) {
  BrokerZenoh *pBrkr = (BrokerZenoh *)arg;
  string topic(sample->key.val, sample->key.len);
  Bytes data(sample->value.val, sample->value.val + sample->value.len);
  pBrkr->_incoming.on({topic, data});
}

BrokerZenoh::BrokerZenoh(Thread &thr, Config &cfg) : BrokerBase(thr, cfg) {
  _zenoh_session = 0;
}

int BrokerZenoh::scout() {
  zn_properties_t *config = zn_config_default();
  z_string_t us_west = z_string_make(US_WEST);
  //  zn_properties_insert(config, ZN_CONFIG_PEER_KEY, us_west);
  zn_properties_insert(config, ZN_CONFIG_LISTENER_KEY, us_west);

  zn_hello_array_t hellos = zn_scout(ZN_ROUTER | ZN_PEER, config, 3000);
  if (hellos.len > 0) {
    for (unsigned int i = 0; i < hellos.len; ++i) {
      zn_hello_t hello = hellos.val[i];
      z_str_array_t locators = hello.locators;
      Bytes pid;
      if (hello.pid.val != NULL) {
        pid = Bytes(hello.pid.val, hello.pid.val + hello.pid.len);
      }
      INFO(" whatami: %s pid : %s ",
           hello.whatami == ZN_ROUTER ? "ZN_ROUTER"
           : hello.whatami == ZN_PEER ? "ZN_PEER"
                                      : "OTHER",
           hexDump(pid, "").c_str());
      for (unsigned int i = 0; i < locators.len; i++) {
        INFO(" locator : %s ", locators.val[i]);
      }
    }
    zn_hello_array_free(hellos);
  } else {
    WARN("Did not find any zenoh process.\n");
  }
  return 0;
}

int BrokerZenoh::init() { return 0; }

int BrokerZenoh::connect(string clientId) {
  zn_properties_t *config = zn_config_default();
  //   zn_properties_insert(config, ZN_CONFIG_LISTENER_KEY,
  //   z_string_make(US_WEST));

  if (_zenoh_session == 0)  // idempotent
    _zenoh_session = zn_open(config);
  INFO("zn_open() %s.", _zenoh_session == 0 ? "failed" : "succeeded");
  _connected = _zenoh_session == 0 ? true : false;
  return _zenoh_session == 0 ? -1 : 0;
}

int BrokerZenoh::disconnect() {
  INFO(" disconnecting.");
  for (auto tuple : _subscribers) {
    zn_undeclare_subscriber(tuple.second->zn_subscriber);
  }
  _subscribers.clear();
  for (auto tuple : _publishers) {
    zn_undeclare_publisher(tuple.second->zn_publisher);
  }
  _publishers.clear();
  _connected = false;
  return 0;
  //  if (_zenoh_session)
  //   zn_close(_zenoh_session);
  // _zenoh_session = NULL;
}

int BrokerZenoh::subscribe(string pattern) {
  INFO(" Zenoh subscriber %s ", pattern.c_str());
  if (_subscribers.find(pattern) == _subscribers.end()) {
    SubscriberStruct *sub = new SubscriberStruct({pattern, 0});
    zn_subscriber_t *zsub =
        zn_declare_subscriber(_zenoh_session, zn_rname(pattern.c_str()),
                              zn_subinfo_default(), subscribeHandler, this);
    sub->zn_subscriber = zsub;
    if (zsub == NULL) {
      WARN(" subscription failed for %s ", pattern.c_str());
      return -1;
    }
    _subscribers.emplace(pattern, sub);
  }
  return 0;
}

int BrokerZenoh::unSubscribe(string pattern) {
  auto it = _subscribers.find(pattern);
  if (it == _subscribers.end()) {
  } else {
    zn_undeclare_subscriber(it->second->zn_subscriber);
  }
  return 0;
}

int BrokerZenoh::newZenohPublisher(string topic) {
  if (_publishers.find(topic) == _publishers.end()) {
    INFO(" Adding Zenoh publisher  : '%s' in %d publishers", topic.c_str(),
         _publishers.size());
    zn_reskey_t reskey = resource(topic);
    zn_publisher_t *pub = zn_declare_publisher(_zenoh_session, reskey);
    if (pub == 0) WARN(" unable to declare publisher '%s'", topic.c_str());
    PublisherStruct *pPub = new PublisherStruct{topic, reskey, pub};
    _publishers.emplace(topic, pPub);
  }
  return 0;
}

int BrokerZenoh::publish(string topic, const Bytes &bs) {
  auto it = _publishers.find(topic);
  if (it != _publishers.end()) {
    //    INFO("publish %d : %s => %s ", id, it->second->pattern.c_str(),
    //    cborDump(bs).c_str());
    int rc =
        zn_write(_zenoh_session, it->second->zn_reskey, bs.data(), bs.size());
    if (rc) WARN("zn_write failed.");
    return rc;
  } else {
    INFO(" publish %s unknown in %d publishers. ", topic, _publishers.size());
    return ENOENT;
  }
}

zn_reskey_t BrokerZenoh::resource(string topic) {
  zn_reskey_t rid =
      zn_rid(zn_declare_resource(_zenoh_session, zn_rname(topic.c_str())));
  return rid;
}

#include <regex>
bool BrokerZenoh::match(string pattern, string topic) {
  std::regex patternRegex(pattern);
  return std::regex_match(topic, patternRegex);
}

/*
vector<PubMsg> BrokerZenoh::query(string uri) {
  vector<PubMsg> result;
  zn_reply_data_array_t replies = zn_query_collect(
      _zenoh_session, zn_rname(uri.c_str()), "", zn_query_target_default(),
      zn_query_consolidation_default());
  for (unsigned int i = 0; i < replies.len; ++i) {
    result.push_back(
        {string(replies.val[i].data.pattern.val,
replies.val[i].data.pattern.len), Bytes(replies.val[i].data.value.val,
               replies.val[i].data.value.val + replies.val[i].data.value.len)});
    printf(">> [Reply handler] received (%.*s, %.*s)\n",
           (int)replies.val[i].data.pattern.len,
replies.val[i].data.pattern.val, (int)replies.val[i].data.value.len,
replies.val[i].data.value.val);
  }
  zn_reply_data_array_free(replies);
  return result;
}
*/