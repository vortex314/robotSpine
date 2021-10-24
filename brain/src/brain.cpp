#include <nlohmann/json.hpp>
using Json = nlohmann::json;
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

#include <BrokerRedisJson.h>

Config loadConfig(int argc, char **argv) {
  Config cfg;
  // defaults
  cfg["serial"]["port"] = "/dev/ttyUSB0";
  cfg["serial"]["baudrate"] = 115200;
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  String s = cfg.dump(2);
  printf("%s\n",s.c_str());
  INFO("%s", s.c_str());

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

  INFO("%s", cfg.dump(2).c_str());
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
  TimerSource timerLatency(workerThread, 1000, true, "ticker");
  TimerSource timerMeta(workerThread, 30000, true, "ticker");
  uint64_t startTime = Sys::millis();

  BrokerRedis broker(workerThread, brokerConfig);
  broker.init();
  int rc = broker.connect("brain");
  TimeoutFlow<uint64_t> fl(workerThread, 2000);

  auto &pubUptime = broker.publisher<uint64_t>("src/brain/system/uptime");
  auto &pubLoopback = broker.publisher<uint64_t>("src/brain/system/loopback");

  timerLatency >> [&](const TimerMsg &) {
    INFO("pub %lu ", Sys::micros());
    pubUptime.on(Sys::millis() - startTime);
    pubLoopback.on(Sys::micros());
  };
  timerMeta >> [&](const TimerMsg &) {
    broker.request("keys *", [&](redisReply *reply) {
      for (int i = 0; i < reply->elements; i++) {
        string key = reply->element[i]->str;
        if (key.rfind("ts:", 0) == 0) {
          INFO(" key[%d] :  %s ", i, key.c_str());
          vector<string> parts = split(key, '/');
          broker.command(
              stringFormat("TS.ALTER %s LABELS node %s object %s property %s",
                           key.c_str(), parts[1].c_str(), parts[2].c_str(),
                           parts[3].c_str())
                  .c_str());
        }
      }
    });
  };

  broker.subscriber<uint64_t>("src/brain/system/loopback") >>
      *new LambdaFlow<uint64_t, uint64_t>(
          [&](uint64_t &out, const uint64_t &in) {
            uint64_t now = Sys::micros();
            out = now - in;
            INFO(" src/brain/system/loopback in : %lu now : %lu delta :%lu ",
                 in, now, out);
            return true;
          }) >>
      broker.publisher<uint64_t>("src/brain/system/latency");

  broker.subscribe("src/*");
  broker.incoming() >> [&](const PubMsg &msg) {
    //    broker.command(stringFormat("SET %s \%b", key.c_str()).c_str(),
    //    bs.data(), bs.size());
//    INFO("%s:%s",msg.topic.c_str(),msg.payload.c_str());
    vector<string> parts = split(msg.topic, '/');
    string key = msg.topic;
    Json json = Json::parse(msg.payload);
    switch (json.type()) {
      case Json::value_t::number_unsigned: {
        uint64_t ui64 = json.get<uint64_t>();
        broker.command(stringFormat("SET %s %ld ", key.c_str(), ui64).c_str());
        broker.command(stringFormat("TS.ADD ts:%s %lu %ld", key.c_str(),
                                    Sys::millis(), ui64)
                           .c_str());
        break;
      }
      case Json::value_t::number_integer: {
        int64_t i64 = json.get<int64_t>();
        broker.command(stringFormat("SET %s %ld ", key.c_str(), i64).c_str());
        broker.command(stringFormat("TS.ADD ts:%s %lu %ld", key.c_str(),
                                    Sys::millis(), i64)
                           .c_str());
        break;
      }
      case Json::value_t::number_float: {
        double d = json.get<double>();
        broker.command(stringFormat("SET %s %f ", key.c_str(), d).c_str());
        broker.command(
            stringFormat("TS.ADD ts:%s %lu %f", key.c_str(), Sys::millis(), d)
                .c_str());
        break;
      }
    }
    /*    double d;
        if (cborDeserializer.fromBytes(msg.payload).begin().get(d).success()) {

        }
        string s;*/
    /*  if (cborDeserializer.fromBytes(msg.payload).begin().get(s).success())
        broker.command(
            stringFormat("SET  %s '%s'", key.c_str(), s.c_str()).c_str());*/
  };

  workerThread.run();
  broker.disconnect();
}
