#include <CborDump.h>
#include <CborReader.h>
#include <CborWriter.h>
#include <cbor.h>
#include <config.h>
#include <log.h>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <util.h>
#include <utility>

using namespace std;

LogS logger;

#include "BrokerRedisJson.h"
#include <Frame.h>
#include <SessionSerial.h>
#include <broker_protocol.h>
const int MsgPublish::TYPE;

//====================================================

const char *CMD_TO_STRING[] = {"B_CONNECT",   "B_DISCONNECT", "B_SUBSCRIBER",
                               "B_PUBLISHER", "B_PUBLISH",    "B_RESOURCE",
                               "B_QUERY"};

#define fatal(message)                                                         \
  {                                                                            \
    LOGW << message << LEND;                                                   \
    exit(-1);                                                                  \
  }

Config loadConfig(int argc, char **argv) {
  Config cfg;
  // defaults
  cfg["serial"]["port"] = "/dev/ttyUSB0";
  cfg["serial"]["baudrate"] = 115200;
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:s:b:")) != -1)
    switch (c) {
    case 'b':
      cfg["serial"]["baudrate"] = atoi(optarg);
      break;
    case 's':
      cfg["serial"]["port"] = optarg;
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

  LOGI << cfg.dump(2) << LEND;
  return cfg;
};

//================================================================

//==========================================================================
int main(int argc, char **argv) {
  LOGI << "Loading configuration." << LEND;
  Config config = loadConfig(argc, argv);
  Thread workerThread("worker");
  Config serialConfig = config["serial"];

  string dstPrefix;
  string srcPrefix;

  SessionSerial serialSession(workerThread, config["serial"]);
  Config brokerConfig = config["broker"];

  INFO(" Launching Redis");
  BrokerRedis broker(workerThread, brokerConfig);
  BrokerRedis brokerProxy(workerThread, brokerConfig);
  string nodeName;
  string portName = config["serial"]["port"];
  serialSession.init();
  serialSession.connect();
  // zSession.scout();
  broker.init();
  broker.connect("serial");
  brokerProxy.init();
  brokerProxy.connect(config["serial"]["port"]);
  // CBOR de-/serialization

  /*serialSession.incoming() >>
      [&](const String &s) { INFO("RXD %s", s.c_str()); };*/

  auto getAnyMsg = new SinkFunction<Bytes>([&](const Bytes &frame) {
    CborReader cborReader(1000);
    int msgType;
    cborReader.fill(frame);
    if (cborReader.checkCrc()) {
//      INFO("RXD hex : %s", hexDump(frame).c_str());
      std::string s; 
      cborReader.parse().toJson(s);
      INFO("RXD cbor: %s", s.c_str());
      if (cborReader.parse().array().get(msgType).ok()) {
        switch (msgType) {
        case B_PUBLISH: {
          std::string topic;
          if (cborReader.get(topic).ok()) {
            String s;
            cborReader.toJson(s);
            broker.publish(topic, s);
          }
          break;
        }
        case B_SUBSCRIBE: {
          std::string topic;
          if (cborReader.get(topic).ok())
            broker.subscribe(topic);
          break;
        }
        case B_NODE: {
          std::string nodeName;
          if (cborReader.get(nodeName).ok()) {
            String topic = "dst/";
            topic += nodeName;
            topic += "/*";
            broker.subscribe(topic);
            topic = "dst/";
            topic += nodeName;
            topic += "-proxy/*";
            brokerProxy.subscribe(topic);
          }
          break;
        }
        }
      }
      cborReader.close();
    } else {
      serialSession.logs().emit(std::string(frame.begin(), frame.end()));
    }
  });

  serialSession.incoming() >> getAnyMsg;

  /* getPubMsg >> [&](const PubMsg &msg) {
     INFO("PUBLISH %s %s ", msg.topic, cborDump(msg.payload).c_str());
   };*/

  SinkFunction<String> redisLogStream([&](const String &bs) {
    static String buffer;
    for (uint8_t b : bs) {
      if (b == '\n') {
        printf("%s%s%s\n", ColorOrange, buffer.c_str(), ColorDefault);
        broker.command("XADD logs * node %s message %s ", nodeName.c_str(),
                       buffer.c_str());
        buffer.clear();
      } else if (b == '\r') { // drop
      } else {
        buffer += (char)b;
      }
    }
  });

  serialSession.logs() >> redisLogStream;

  /*
        ::write(1, ColorOrange, strlen(ColorOrange));
      ::write(1, bs.data(), bs.size());
      ::write(1, ColorDefault, strlen(ColorDefault));
  */
  broker.incoming() >>
      new LambdaFlow<PubMsg, String>([&](String &msg, const PubMsg &pub) {
        /* Json json = pub.payload;
         Json v = {B_PUBLISH, pub.topic, json};
         msg = v.dump();*/
        return false;
      }) >>
      new LambdaFlow<String, Bytes>(
          [&](Bytes &msg, const String &bs) { return false; }) >>
      serialSession.outgoing();

  workerThread.run();
}
