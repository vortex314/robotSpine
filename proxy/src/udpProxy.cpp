#include <ArduinoJson.h>
#include <config.h>
#include <log.h>
#include <stdio.h>
#include <util.h>

#include <thread>
#include <unordered_map>
#include <utility>

using namespace std;

LogS logger;
#ifdef BROKER_ZENOH
#include <BrokerZenoh.h>
#endif
#ifdef BROKER_REDIS
#include <BrokerRedis.h>
#endif
#include <CborDump.h>
#include <Frame.h>
#include <ReflectFromCbor.h>
#include <ReflectToCbor.h>
#include <ReflectToDisplay.h>
#include <SessionSerial.h>
#include <SessionUdp.h>
#include <broker_protocol.h>
const int MsgPublish::TYPE;

//====================================================

const char *CMD_TO_STRING[] = {"B_CONNECT",   "B_DISCONNECT", "B_SUBSCRIBER",
                               "B_PUBLISHER", "B_PUBLISH",    "B_RESOURCE",
                               "B_QUERY"};
StaticJsonDocument<10240> doc;

#define fatal(message)       \
  {                          \
    LOGW << message << LEND; \
    exit(-1);                \
  }

Config loadConfig(int argc, char **argv) {
  Config cfg = doc.to<JsonObject>();
  // defaults
  cfg["serial"]["port"] = "/dev/ttyUSB0";
  cfg["serial"]["baudrate"] = 115200;
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:s:b:")) != -1) switch (c) {
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

  string sCfg;
  serializeJson(doc, sCfg);
  LOGI << sCfg << LEND;
  return cfg;
};

bool writeBytesToFile(string fileName, const Bytes &data) {
  ofstream wf(fileName, ios::out | ios::binary);
  if (!wf) {
    WARN("Cannot open file : '%s' !", fileName.c_str());
    return false;
  }
  wf.write((char *)data.data(), data.size());
  if (!wf.good()) {
    WARN("Error occurred at writing time!: '%s' !", fileName.c_str());
    wf.close();
    return false;
  }
  wf.close();
  return wf.good();
}

bool flashFileToDevice(Sink<Bytes>& logStream,string fileName, string port, uint32_t baudrate) {
  string cmd= "ls -l  ";
  char buffer[128];
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    WARN("popen() failed :  %s  ", strerror(errno));
    return false;
  }
  try {
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
      INFO("LOG ===> %s ",buffer);
      Bytes bs((uint8_t)*buffer,(uint8_t)*buffer+strlen(buffer));
      logStream.on(bs);
    }
  } catch (...) {
    pclose(pipe);
    return false;
  }
  pclose(pipe);
  return true;
}

bool flashFileToDevice2(string fileName, string port, uint32_t baudrate) {
  INFO("  esptool.py --port %s write_flash 0x0000 %s --baud %u ", port.c_str(),
       fileName.c_str(), baudrate);
  string python = "/home/lieven/.platformio/penv/bin/python";
  string esptool =
      "/home/lieven/.platformio/packages/tool-esptoolpy/esptool.py";
  string bootloader =
      "/home/lieven/.platformio/packages/framework-arduinoespressif32/tools/"
      "sdk/bin/bootloader_dio_40m.bin";
  string partitions =
      "/home/lieven/workspace/broker-proxy/arduino_client/.pio/build/"
      "nodemcu-32s/partitions.bin";
  string boot_app0 =
      "/home/lieven/.platformio/packages/framework-arduinoespressif32/tools/"
      "partitions/boot_app0.bin";
  string firmware = ".pio/build/nodemcu-32s/firmware.bin";
  string command = stringFormat(
      "%s %s --chip esp32 --port %s --baud %u "
      " --before default_reset --after hard_reset write_flash -z "
      " --flash_mode dio --flash_freq 40m --flash_size detect "
      " 0x1000 %s "
      " 0x8000 %s "
      " 0xe000 %s "
      " 0x10000 %s",
      python, esptool, port, baudrate, bootloader, partitions, boot_app0,
      firmware);

  return true;
}

//================================================================

//==========================================================================
int main(int argc, char **argv) {
  LOGI << "Loading configuration." << LEND;
  Config config = loadConfig(argc, argv);
  Thread workerThread("worker");
  Config serialConfig = config["serial"];

  string dstPrefix;
  string srcPrefix;

  SessionAbstract *session;
  if (config["serial"])
    session = new SessionSerial(workerThread, config["serial"]);
  else if (config["udp"])
    session = new SessionUdp(workerThread, config["udp"]);
  else
    fatal(" no interface specified.");
  Config brokerConfig = config["broker"];

  INFO(" Launching Redis");
  BrokerRedis broker(workerThread, brokerConfig);
  BrokerRedis brokerProxy(workerThread, brokerConfig);
  CborDeserializer fromCbor(1024);
  CborSerializer toCbor(1024);
  string nodeName;
  string portName = config["serial"]["port"];
  session->init();
  session->connect();
  // zSession.scout();
  broker.init();
  broker.connect("serial");
  brokerProxy.init();
  brokerProxy.connect(config["serial"]["port"]);
  // CBOR de-/serialization

  session->incoming() >>
      [&](const bytes &bs) { INFO("RXD %s", cborDump(bs).c_str()); };

  // filter commands from uC
  auto getPubMsg =
      new LambdaFlow<Bytes, PubMsg>([&](PubMsg &msg, const Bytes &frame) {
        int msgType;
        return fromCbor.fromBytes(frame)
                   .begin()
                   .get(msgType)
                   .get(msg.topic)
                   .get(msg.payload)
                   .end()
                   .success() &&
               msgType == B_PUBLISH;
      });

  auto getSubMsg =
      new LambdaFlow<Bytes, SubMsg>([&](SubMsg &msg, const Bytes &frame) {
        int msgType;
        return fromCbor.fromBytes(frame)
                   .begin()
                   .get(msgType)
                   .get(msg.pattern)
                   .success() &&
               msgType == B_SUBSCRIBE;
      });

  auto getNodeMsg =
      new LambdaFlow<Bytes, NodeMsg>([&](NodeMsg &msg, const Bytes &frame) {
        int msgType;
        return fromCbor.fromBytes(frame)
                   .begin()
                   .get(msgType)
                   .get(msg.node)
                   .success() &&
               msgType == B_NODE;
      });

  session->incoming() >> getPubMsg >> [&](const PubMsg &msg) {
    INFO("PUBLISH %s %s ", msg.topic.c_str(), cborDump(msg.payload).c_str());
    broker.publish(msg.topic, msg.payload);
  };
  session->incoming() >> getSubMsg >>
      [&](const SubMsg &msg) { broker.subscribe(msg.pattern); };

  session->incoming() >> getNodeMsg >> [&](const NodeMsg &msg) {
    INFO("NODE %s", msg.node.c_str());
    nodeName = msg.node;
    std::string topic = "dst/";
    topic += msg.node;
    topic += "/*";
    broker.subscribe(topic);
    topic = "dst/";
    topic += msg.node;
    topic += "-proxy/*";
    brokerProxy.subscribe(topic);
  };

  /* getPubMsg >> [&](const PubMsg &msg) {
     INFO("PUBLISH %s %s ", msg.topic, cborDump(msg.payload).c_str());
   };*/

  SinkFunction<Bytes> redisLogStream([&](const Bytes &bs) {
    static string buffer;
    for (uint8_t b : bs) {
      if (b == '\n') {
        broker.command("XADD logs * node %s message %s ", nodeName.c_str(),
                       buffer.c_str());
        buffer.clear();
      } else if (b == '\r') {  // drop
      } else {
        buffer += (char)b;
      }
    }
  });

  session->logs() >> redisLogStream;

  broker.incoming() >>
      new LambdaFlow<PubMsg, Bytes>([&](Bytes &bs, const PubMsg &msg) {
        bs = toCbor.begin()
                 .add(MsgPublish::TYPE)
                 .add(msg.topic)
                 .add(msg.payload)
                 .end()
                 .toBytes();
        return toCbor.success();
      }) >>
      session->outgoing();

  brokerProxy.incoming() >> [&](const PubMsg &msg) {
    string dstPrefix = stringFormat("dst/%s-proxy/", nodeName.c_str());
    if (msg.topic == (dstPrefix + "prog/flash")) {
      INFO(" received flash image : %d bytes ", msg.payload.size());
      session->disconnect();
      if (writeBytesToFile("image.bin", msg.payload)) {
        flashFileToDevice(redisLogStream,"image.bin", portName, 921600);
      }
      session->connect();
    }
  };

  workerThread.run();
  delete session;
}
