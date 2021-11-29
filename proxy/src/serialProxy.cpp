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

  serialSession.incoming() >>
      [&](const Bytes &s) { 
//        INFO("RXD %s", hexDump(s).c_str()); 
        };

  auto getAnyMsg = new SinkFunction<Bytes>([&](const Bytes &frame) {
    CborReader cborReader(1000);
    int msgType;
    cborReader.fill(frame);
    if (cborReader.checkCrc()) {
 //     INFO("RXD good Crc hex[%d] : %s", frame.size(), hexDump(frame).c_str());
      std::string s;
      cborReader.parse().toJson(s);
      Bytes bs = cborReader.toBytes();
  //    INFO("RXD cbor[%d]: %s", bs.size(), hexDump(bs).c_str());
      INFO("RXD cbor[%d]: %s", bs.size(), s.c_str());
      if (cborReader.parse().array().get(msgType).ok()) {
        switch (msgType) {
        case B_PUBLISH: {
          std::string topic;
          if (cborReader.get(topic).ok()) {
            String s;
            cborReader.toJson(s);
            broker.publish(topic, s);
          } else {
            INFO(" invalid CBOR publish");
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
      if ( frame.back() != '\n') INFO(" invalid CRC [%d] %s",frame.size(),hexDump(frame).c_str());
      serialSession.logs().emit(std::string(frame.begin(), frame.end()));
    }
  });

  serialSession.incoming() >> getAnyMsg;

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
      new LambdaFlow<PubMsg, Bytes>([&](Bytes &msg, const PubMsg &pub) {
        CborWriter writer(100);
        writer.reset().array().add(B_PUBLISH).add(pub.topic);
        cborAddJson(writer, pub.payload);
        writer.close();
        writer.addCrc();
        msg = writer.bytes();
        return true;
      }) >>
      serialSession.outgoing();

  workerThread.run();
}
