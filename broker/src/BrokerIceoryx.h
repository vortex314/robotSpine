#ifndef _ICE_H_
#define _ICI_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "iceoryx_hoofs/posix_wrapper/signal_handler.hpp"
#include "iceoryx_posh/popo/listener.hpp"
#include "iceoryx_posh/popo/publisher.hpp"
#include "iceoryx_posh/popo/subscriber.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"

#include "BrokerBase.h"
#include "limero.h"
#include <CborDump.h>
#include <util.h>

using namespace std;
using namespace iox::capro;


typedef vector<uint8_t> Bytes;

struct IceBytes {
  uint32_t size;
  uint8_t data[1024];
  IceBytes(Bytes &);
  IceBytes() { size = 0; };
  Bytes toBytes() const;
  void operator=(const Bytes &);
};

class BrokerIceoryx  : public BrokerBase {
public:
  static iox::posix::Semaphore _shutdownSemaphore;
  unordered_map<string, iox::popo::Publisher<IceBytes> *> _publishers;
  unordered_map<string, iox::popo::Subscriber<IceBytes> *> _subscribers;

public:
  BrokerIceoryx(Thread &, Config );
  int init();
  int connect(string);
  int disconnect();
  int publish(string, const Bytes &);
  int onSubscribe(SubscribeCallback);
  int unSubscribe(string);
  int subscribe(string);
  bool match(string pattern, string source);
  ServiceDescription makeServiceDescription(string topic) ;
};

#endif