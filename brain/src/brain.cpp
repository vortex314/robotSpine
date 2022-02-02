#include <ArduinoJson.h>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;
#include <Log.h>
#include <StringUtility.h>
#include <limero.h>
#include <stdio.h>
#include <unistd.h>
// #include <util.h>

#include <thread>
#include <unordered_map>
#include <utility>

using namespace std;

Log logger;
StaticJsonDocument<10240> jsonDoc;

#include <BrokerRedisJson.h>

typedef JsonObject Config;

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

template <typename T>
T scale(T x, T x1, T x2, T y1, T y2) {
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

//==========================================================================
int main(int argc, char **argv) {
  Config config = jsonDoc.as<JsonObject>();

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

  broker.subscriber<int32_t>("src/joystick/axis/1") >>
      *new LambdaFlow<int32_t, int32_t>([&](int32_t &out, const int32_t &in) {
        out = -scale(in, -32768, +32768, -1000, +1000);
        return true;
      }) >>
      broker.publisher<int32_t>("dst/hover/motor/speed");

  broker.subscriber<int32_t>("src/joystick/axis/0") >>
      *new LambdaFlow<int32_t, int32_t>([&](int32_t &out, const int32_t &in) {
        out = scale(in, -32768, +32768, -1000, +1000);
        return true;
      }) >>
      broker.publisher<int32_t>("dst/hover/motor/steer");

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
    String object = parts[2];
    String property = parts[3];
    string key = msg.topic;
    Json json = msg.payload;
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
    if (object == "gps" && property == "location") {
      Json location = msg.payload;
      INFO("%s", location.dump().c_str());
      String command = stringFormat("XADD str:pos * lon %f lat %f ",
                                    location["lon"].get<double>(),
                                    location["lat"].get<double>());
      broker.command(command.c_str());
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
