#include <ArduinoJson.h>
#include <config.h>
#include <limero.h>
#include <log.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>

#include <thread>
#include <unordered_map>
#include <utility>

using namespace std;

LogS logger;

#include <BrokerRedis.h>
#include <CborDeserializer.h>
#include <CborDump.h>
#include <CborSerializer.h>
StaticJsonDocument<10240> doc;

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

template <typename T>
class TimeoutFlow : public LambdaFlow<T, bool>, public Actor {
  TimerSource _watchdogTimer;

 public:
  TimeoutFlow(Thread &thr, uint32_t delay)
      : Actor(thr), _watchdogTimer(thr, delay, true, "watchdog") {
    this->emit(false);
    this->lambda([&](bool &out, const T &t) {
      _watchdogTimer.reset();
      this->emit(true);
      return true;
    });
    _watchdogTimer >> [&](const TimerMsg &) { this->emit(false); };
  }
};

//==========================================================================
int main(int argc, char **argv) {
  Config config = loadConfig(argc, argv);
  Thread workerThread("worker");
  Config brokerConfig = config["broker"];
  TimerSource pubTimer(workerThread, 2000, true, "pubTimer");
  CborSerializer cborSerializer(1024);
  CborDeserializer cborDeserializer(1024);
  TimerSource ticker(workerThread, 3000, true, "ticker");

  BrokerRedis broker(workerThread, brokerConfig);
  broker.init();
  int rc = broker.connect("brain");
  TimeoutFlow<uint64_t> fl(workerThread, 2000);

  auto &pub = broker.publisher<uint64_t>("src/brain/system/uptime");

  ticker >> [&](const TimerMsg &) {
    INFO("ticker");
    pub.on(Sys::millis());
    broker.request("keys *", [&](redisReply *reply) {
      for (int i = 0; i < reply->elements; i++) {
        string key = reply->element[i]->str;
        if (key.rfind("ts:", 0) == 0) {
          INFO(" key[%d] :  %s ", i, key.c_str());
          vector<string> parts = split(key, '/');
          broker.command(
              stringFormat("ts.alter %s labels node %s object %s property %s",
                           key.c_str(), parts[1].c_str(), parts[2].c_str(),
                           parts[3].c_str())
                  .c_str());
        }
      }
    });
  };

  broker.subscriber<int>("") >>
      *new LambdaFlow<int, uint64_t>([&](uint64_t &out, const int &) {
        out = Sys::micros();
        return true;
      }) >>
      broker.publisher<uint64_t>("src/brain/system/uptime");

  broker.subscriber<uint64_t>("src/brain/system/uptime") >>
      *new LambdaFlow<uint64_t, uint64_t>([&](uint64_t &out,
                                              const uint64_t &in) {
        out = Sys::micros() - in;
        LOGI << " recv ts " << in << " latency : " << out << " usec " << LEND;
        return true;
      }) >>
      broker.publisher<uint64_t>("src/brain/system/latency");

  broker.subscriber<bool>("src/stellaris/system/alive") >>
      [&](const bool &b) { INFO("alive."); };

  broker.subscribe("src/*");
  broker.incoming() >> [&](const PubMsg &msg) {
    //    broker.command(stringFormat("SET %s \%b", key.c_str()).c_str(),
    //    bs.data(), bs.size());
    vector<string> parts = split(msg.topic, '/');
    string key = msg.topic;
    int64_t i64;
    if (cborDeserializer.fromBytes(msg.payload).begin().get(i64).success()) {
      broker.command(stringFormat("SET %s %ld ", key.c_str(), i64).c_str());
      broker.command(
          stringFormat("TS.ADD ts:%s %lu %ld", key.c_str(), Sys::millis(), i64)
              .c_str());
    }
    double d;
    if (cborDeserializer.fromBytes(msg.payload).begin().get(d).success()) {
      broker.command(stringFormat("SET %s %f ", key.c_str(), d).c_str());
      broker.command(
          stringFormat("TS.ADD ts:%s %lu %f", key.c_str(), Sys::millis(), d)
              .c_str());
    }
    string s;
    if (cborDeserializer.fromBytes(msg.payload).begin().get(s).success())
      broker.command(
          stringFormat("SET  %s \"%s\" ", key.c_str(), s.c_str()).c_str());
  };


  workerThread.run();
  broker.disconnect();
}
