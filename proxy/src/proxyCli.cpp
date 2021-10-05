#include <ArduinoJson.h>
#include <BrokerRedis.h>
#include <CborDeserializer.h>
#include <CborDump.h>
#include <CborSerializer.h>
#include <Frame.h>
#include <config.h>
#include <log.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>

#include <thread>
#include <unordered_map>
#include <utility>

using namespace std;
LogS logger;

Config loadConfig(JsonDocument &doc, int argc, char **argv) {
  Config cfg = doc.to<JsonObject>();
  // defaults
  cfg["serial"]["port"] = "/dev/ttyUSB0";
  cfg["serial"]["baudrate"] = 115200;
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  // override args
  int c;
  while ((c = getopt(argc, argv, "n:f:")) != -1) switch (c) {
      case 'f':
        cfg["file"] = atoi(optarg);
        break;
      case 'n':
        cfg["node"] = atoi(optarg);
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

bool readBytesFromFile(Bytes &bin, string fileName) {
  ifstream rf(fileName, ios::in | ios::binary);
  if (!rf) {
    WARN("Cannot open file : '%s' !", fileName.c_str());
    return false;
  }
  rf.seekg(0, rf.end);
  uint32_t size = rf.tellg();
  rf.seekg(0, rf.beg);
  bin.resize(size);
  rf.read((char *)bin.data(), size);
  rf.close();
  return rf.good();
}

int main(int argc, char **argv) {
  LOGI << "Loading configuration." << LEND;
  StaticJsonDocument<10240> json;

  Thread workerThread("worker");
  TimerSource ticker(workerThread, 5000, true, "ticker");
  Config config = loadConfig(json, argc, argv);
  Config brokerConfig = config["broker"];
  BrokerRedis broker(workerThread, brokerConfig);
  CborDeserializer fromCbor(10000000);
  CborSerializer toCbor(10000000);

  string fileName = config["file"] | "firmware.bin";
  string node = config["node"] | "esp32";
  string flashTopic = stringFormat("dst/%s-proxy/prog/flash", node.c_str());

  broker.connected >> [&](const bool &) {
    Bytes binary;
    if (readBytesFromFile(binary, fileName))
      broker.publish(flashTopic, toCbor.begin().add(binary).end().toBytes());
      broker.disconnect();
      exit(0);
  };
  workerThread.run();
}