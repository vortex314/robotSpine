#include <config.h>
#include <log.h>
#include <stdio.h>
#include <util.h>

#include <nlohmann/json.hpp>
#include <thread>
#include <unordered_map>
#include <utility>
using Json = nlohmann::json;
using namespace std;

LogS logger;

#include <Frame.h>
#include <SessionSerial.h>
#include <broker_protocol.h>

#include "BrokerRedisJson.h"
const int MsgPublish::TYPE;

//====================================================

const char *CMD_TO_STRING[] = {"B_CONNECT",   "B_DISCONNECT", "B_SUBSCRIBER",
                               "B_PUBLISHER", "B_PUBLISH",    "B_RESOURCE",
                               "B_QUERY"};

#define fatal(message)       \
  {                          \
    LOGW << message << LEND; \
    exit(-1);                \
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

  LOGI << cfg.dump(2) << LEND;
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

bool flashFileToDevice(Sink<Bytes> &logStream, string fileName, string port,
                       uint32_t baudrate) {
  string cmd = "ls -l  ";
  char buffer[128];
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    WARN("popen() failed :  %s  ", strerror(errno));
    return false;
  }
  try {
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
      INFO("LOG ===> %s ", buffer);
      Bytes bs((uint8_t)*buffer, (uint8_t)*buffer + strlen(buffer));
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
      [&](const String &s) { INFO("RXD %s", s.c_str()); };

  // filter commands from uC
  auto getAnyMsg = new SinkFunction<String>([&](const String &frame) {
    int msgType;
    Json json = Json::parse(frame);
    INFO("%s => %s ", frame.c_str(), json.dump().c_str());
    if (json.type() == Json::value_t::array) {
      int msgType = json[0].get<int>();
      String arg1 = json[1].get<String>();
      switch (msgType) {
        case B_PUBLISH: {
          Json arg2 = json[2];
          broker.publish(arg1, arg2.dump());
          break;
        }
        case B_SUBSCRIBE: {
          broker.subscribe(arg1);
          break;
        }
        case B_NODE: {
          String topic = "dst/";
          topic += arg1;
          topic += "/*";
          broker.subscribe(topic);
          topic = "dst/";
          topic += arg1;
          topic += "-proxy/*";
          brokerProxy.subscribe(topic);
          break;
        }
      };
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
      } else if (b == '\r') {  // drop
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
        Json json = Json::parse(pub.payload);
        Json v = {B_PUBLISH, pub.topic, json};
        msg = v.dump();
        return true;
      }) >>
      serialSession.outgoing();

  workerThread.run();
}
