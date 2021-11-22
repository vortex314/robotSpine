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
  QueueFlow<Bytes> _outgoing;
  String nodeName;

 public:
  ClientProxy(Thread &thread, Config config, UdpAddress source)
      : Actor(thread),
        _broker(thread, config),
        _source(source),
        _incoming(10, "incoming"),
        _outgoing(5, "outgoing") {
    INFO(" created clientProxy %s ", _source.toString().c_str());
  };
  void init() {
    _broker.init();
    _broker.connect(_source.toString().c_str());
    _incoming >> [&](const Bytes &bs) {
      INFO(" %s client rxd %s ", _source.toString().c_str(),
           hexDump(bs).c_str());
    };

    auto getAnyMsg = new SinkFunction<Bytes>([&](const Bytes &frame) {
      CborReader cborReader(1000);
      int msgType;
      cborReader.fill(frame);
      if (cborReader.checkCrc()) {
        //     INFO("RXD good Crc hex[%d] : %s", frame.size(),
        //     hexDump(frame).c_str());
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
                _broker.publish(topic, s);
              } else {
                INFO(" invalid CBOR publish");
              }
              break;
            }
            case B_SUBSCRIBE: {
              std::string topic;
              if (cborReader.get(topic).ok()) _broker.subscribe(topic);
              break;
            }
            case B_NODE: {
              std::string nodeName;
              if (cborReader.get(nodeName).ok()) {
                String topic = "dst/";
                topic += nodeName;
                topic += "/*";
                _broker.subscribe(topic);
              }
              break;
            }
          }
        }
        cborReader.close();
      } else {
          INFO(" invalid CRC [%d] %s", frame.size(), hexDump(frame).c_str());
        //   serialSession.logs().emit(std::string(frame.begin(), frame.end()));
      }
    });

    _incoming >> getAnyMsg;

    SinkFunction<String> redisLogStream([&](const String &bs) {
      static String buffer;
      for (uint8_t b : bs) {
        if (b == '\n') {
          printf("%s%s%s\n", ColorOrange, buffer.c_str(), ColorDefault);
          _broker.command("XADD logs * node %s message %s ", nodeName.c_str(),
                          buffer.c_str());
          buffer.clear();
        } else if (b == '\r') {  // drop
        } else {
          buffer += (char)b;
        }
      }
    });

    // serialSession.logs() >> redisLogStream;

    /*
          ::write(1, ColorOrange, strlen(ColorOrange));
        ::write(1, bs.data(), bs.size());
        ::write(1, ColorDefault, strlen(ColorDefault));
    */
    _broker.incoming() >>
        new LambdaFlow<PubMsg, Bytes>([&](Bytes &msg, const PubMsg &pub) {
          CborWriter writer(100);
          writer.reset().array().add(B_PUBLISH).add(pub.topic);
          cborAddJson(writer, pub.payload);
          writer.close();
          writer.addCrc();
          msg = writer.bytes();
          return true;
        }) >>
        _outgoing;
  }
  Sink<Bytes> &incoming() { return _incoming; }
  Source<Bytes> &outgoing() { return _outgoing; }
  UdpAddress src() { return _source; }
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
  std::map<UdpAddress, ClientProxy *> clients;

  udpSession.init();
  udpSession.connect();
  brokerProxy.init();
  brokerProxy.connect("udp-proxy");
  UdpAddress serverAddress;
  UdpAddress::fromUri(serverAddress, "0.0.0.0:9999");

  udpSession.recv() >> [&](const UdpMsg &udpMsg) {
    INFO(" UDP RXD %s => %s ", udpMsg.src.toString().c_str(),
         udpMsg.dst.toString().c_str());
    UdpAddress src = udpMsg.src;
    ClientProxy *clientProxy;
    auto it = clients.find(src);
    if (it == clients.end()) {
      clientProxy = new ClientProxy(workerThread, brokerConfig, src);
      clientProxy->init();
      clientProxy->outgoing() >> [&](const Bytes &bs) {
        UdpMsg msg;
        msg.message = bs;
        msg.dst = clientProxy->src();
        msg.src = serverAddress;
        udpSession.send().on(msg);
      };
      clients.emplace(src, clientProxy);
    } else {
      clientProxy = it->second;
    }
    clientProxy->incoming().on(udpMsg.message);
    // create new instance for broker connection
    // connect instrance to UdpMsg Stream
  };

  workerThread.run();
}
