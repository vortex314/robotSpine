#include <BrokerRedisJson.h>
#include <CborDump.h>
#include <CborReader.h>
#include <CborWriter.h>
#include <Frame.h>
#include <SessionUdp.h>
#include <broker_protocol.h>
#include <cbor.h>
#include <config.h>
#include <log.h>
#include <stdio.h>
#include <util.h>

#include <thread>
#include <unordered_map>
#include <utility>

using namespace std;

LogS logger;

const int MsgPublish::TYPE;

//====================================================

const char *CMD_TO_STRING[] = {"B_CONNECT",   "B_DISCONNECT", "B_SUBSCRIBER",
                               "B_PUBLISHER", "B_PUBLISH",    "B_RESOURCE",
                               "B_QUERY"};

CborWriter &cborAddJson(CborWriter &writer, Json v) {
  if (v.is_number_integer()) {
    writer.add((int)v);
  } else if (v.is_string()) {
    writer.add((std::string)v);
  } else if (v.is_number_float()) {
    writer.add((double)v);
  } else if (v.is_boolean()) {
    writer.add((bool)v);
  } else {
    writer.add("====================");
  }
  return writer;
}

#define fatal(message)       \
  {                          \
    LOGW << message << LEND; \
    exit(-1);                \
  }

Config loadConfig(int argc, char **argv) {
  Config cfg;
  // defaults
  cfg["udp"]["port"] = 9999;
  cfg["udp"]["net"] = "0.0.0.0";
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:s:u:")) != -1) switch (c) {
      case 'u':
        cfg["udp"]["port"] = atoi(optarg);
        break;
      case 's':
        cfg["udp"]["net"] = optarg;
        break;
      case 'h':
        cfg["broker"]["host"] = optarg;
        break;
      case 'p':
        cfg["broker"]["port"] = atoi(optarg);
        break;
      case '?':
        printf("Usage %s -h <host> -p <port> -s <serial_port> -b <baudrate>\n",
               argv[0]);
        break;
      default:
        WARN("Usage %s -h <host> -p <port> -s <serial_port> -b <baudrate>\n",
             argv[0]);
        abort();
    }

  INFO("%s", cfg.dump(2).c_str());
  return cfg;
};

//================================================================

class ClientProxy : public Actor {
  BrokerRedis _broker;
  UdpAddress _source;
  QueueFlow<Bytes> _incoming;

 public:
  ClientProxy(Thread &thread, Config config, UdpAddress source)
      : Actor(thread),
        _broker(thread, config),
        _source(source),
        _incoming(10, "incoming"){};
  void init() {
    _broker.init();
    _broker.connect(_source.toString().c_str());
  }
  Sink<Bytes> &incoming() { return _incoming; }
};

//==========================================================================
int main(int argc, char **argv) {
  LOGI << "Loading configuration." << LEND;
  Config config = loadConfig(argc, argv);
  Thread workerThread("worker");
  Config udpConfig = config["udp"];

  SessionUdp udpSession(workerThread, config["udp"]);

  Config brokerConfig = config["broker"];
  BrokerRedis brokerProxy(workerThread, brokerConfig);
  std::unordered_map<UdpAddress, ClientProxy *> clients;

  udpSession.init();
  udpSession.connect();
  brokerProxy.init();
  brokerProxy.connect("udp-proxy");

  udpSession.recv() >> [&](const UdpMsg &udpMsg) {
    UdpAddress src = udpMsg.src;
    ClientProxy *clientProxy ;//= clients.find(src);
    if (clientProxy != NULL) {
      clientProxy = new ClientProxy(workerThread, brokerConfig, src);
      clients.emplace(src, clientProxy);
    }

    // create new instance for broker connection
    // connect instrance to UdpMsg Stream
    INFO("UDP RXD %s => %s ", udpMsg.src.toString().c_str(),
         hexDump(udpMsg.message).c_str());
  };

  workerThread.run();
}
