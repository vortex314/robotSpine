#include <BrokerIceoryx.h>

IceBytes::IceBytes(Bytes &bs) {
  memcpy(data, bs.data(), bs.size());
  size = bs.size();
}

void IceBytes::operator=(const Bytes &bs) {
  memcpy(data, bs.data(), bs.size());
  size = bs.size();
}

Bytes IceBytes::toBytes() const {
  Bytes b;
  for(int i=0;i<size;i++) {
    b.push_back(data[i]);
  }
  return b;
}

iox::posix::Semaphore BrokerIceoryx::_shutdownSemaphore;
/*
vector<string> split(const string &s, char seperator) {
  vector<string> output;

  string::size_type prev_pos = 0, pos = 0;

  while ((pos = s.find(seperator, pos)) != string::npos) {
    string substring(s.substr(prev_pos, pos - prev_pos));
    output.push_back(substring);
    prev_pos = ++pos;
  }
  output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word
  return output;
}*/
/*
static void sigHandler(int f_sig IOX_MAYBE_UNUSED) {
  BrokerIceoryx::_shutdownSemaphore.post().or_else([](auto) {
    std::cerr << "unable to call post on shutdownSemaphore - semaphore corrupt?"
              << std::endl;
    std::exit(EXIT_FAILURE);
  });
}*/

BrokerIceoryx::BrokerIceoryx(Thread& thread,Config config) : BrokerBase(thread,config) {
  string name =  config["name"] | "clientIceoryxUnknown";
  iox::runtime::PoshRuntime::initRuntime(
      iox::RuntimeName_t(iox::cxx::TruncateToCapacity, name));
}

int BrokerIceoryx::init() {
  _shutdownSemaphore = iox::posix::Semaphore::create(
                           iox::posix::CreateUnnamedSingleProcessSemaphore, 0U)
                           .value();
  return 0;
}

  int BrokerIceoryx::connect(string){ return 0;}
  int BrokerIceoryx::disconnect() {return 0;}
//========================================================================
ServiceDescription BrokerIceoryx::makeServiceDescription(string topic) {
  vector<string> element = split(topic, '/');
  IdString_t service ;
  IdString_t instance ;
  IdString_t event ;
  if (element.size() > 1 && element[1].size())
    service.unsafe_assign(element[1]);
  if (element.size() > 2 && element[2].size())
    instance.unsafe_assign(element[2]);
  if (element.size() > 3 && element[3].size())
    event.unsafe_assign(element[3]);
  ServiceDescription serviceDescription{service, instance, event};
  return serviceDescription;
}
//========================================================================
int BrokerIceoryx::publish(string topic, const Bytes& data) {
  INFO(" publish %s : [%d]",topic.c_str(),data.size());
  iox::popo::Publisher<IceBytes> *pp;
  auto it = _publishers.find(topic);
  if (it == _publishers.end()) {
    ServiceDescription sd = makeServiceDescription(topic);
    pp = new iox::popo::Publisher<IceBytes>(sd);
    _publishers.emplace(topic, pp);
  } else {
    pp = it->second;
  }
  auto loan = pp->loan();
  if (!loan.has_error()) {
    auto &sample = loan.value();
    *sample = data;
    sample.publish();
  }
  return 0;
}
//========================================================================
void onSampleReceivedCallback(iox::popo::Subscriber<IceBytes> *subscriber) {
  subscriber->take().and_then([subscriber](auto &x) {
    auto sd = subscriber->getServiceDescription();
    std::cout << "received: " << sd.getServiceIDString() << "/"
              << sd.getInstanceIDString() << "/" << sd.getEventIDString()
              << std::endl;
    Bytes dt = x->toBytes();
    INFO("CBOR RXD" , cborDump(dt).c_str());
  });
}
//========================================================================
int BrokerIceoryx::subscribe(string topic) {
  iox::popo::Listener *plistener;
  iox::popo::Subscriber<IceBytes> *ps;
  auto it = _subscribers.find(topic);
  if (it == _subscribers.end()) {
    ServiceDescription sd = makeServiceDescription(topic);

    ps = new iox::popo::Subscriber<IceBytes>(sd);
    _subscribers.emplace(topic, ps);
  } else {
    ps = it->second;
  }
  const ServiceDescription psd = ps->getServiceDescription();
  INFO( "SUB: %s/%s/%s " , psd.getServiceIDString().c_str() , psd.getInstanceIDString().c_str(),psd.getEventIDString().c_str());
  plistener = new iox::popo::Listener();
  plistener
      ->attachEvent(
          *ps, iox::popo::SubscriberEvent::DATA_RECEIVED,
          iox::popo::createNotificationCallback(onSampleReceivedCallback))
      .or_else([](auto) {
        std::cerr << "unable to attach subscriber" << std::endl;
        std::exit(EXIT_FAILURE);
      });
  return 0;
}

  int BrokerIceoryx::unSubscribe(string){
    return 0;
  }


#include <regex>
bool BrokerIceoryx::match(string pattern, string topic) {
  std::regex patternRegex(pattern);
  return std::regex_match(topic, patternRegex);
}