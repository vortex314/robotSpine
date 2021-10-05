#include <ArduinoJson.h>
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
#ifdef BROKER_ZENOH
#include <BrokerZenoh.h>
#endif
#ifdef BROKER_REDIS
#include <BrokerRedis.h>
#endif
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
  while ((c = getopt(argc, argv, "f:e:h:p:")) != -1) switch (c) {
      case 'e':
        cfg["elastic"]["host"] = optarg;
        break;
      case 'f':
        cfg["elastic"]["port"] = atoi(optarg);
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

#include <curl/curl.h>

class ElasticIndex : public Actor {
  string _index;
  QueueFlow<string> _putIndex;
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers = NULL;
  string _url, _host;
  uint16_t _port;

 public:
  ElasticIndex(Thread &thread, Config config, string name)
      : Actor(thread), _index(name), _putIndex(20) {
    _putIndex.async(thread);
    _host = config["host"] | "localhost";
    _port = config["port"] | 9200;
    _url = stringFormat("http://%s:%d/%s/log?pipeline=add-timestamp",
                        _host.c_str(), _port, _index.c_str());
  };
  static size_t write_data(void *buffer, size_t size, size_t nmemb,
                           void *userp) {
    return size * nmemb;
  }
  bool init() {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl = curl_easy_init();
    CURLcode rc;
    if ((rc = curl_easy_setopt(curl, CURLOPT_VERBOSE, 0)) == CURLE_OK &&
        (rc = curl_easy_setopt(curl, CURLOPT_URL, _url.c_str())) == CURLE_OK &&
        (rc = curl_easy_setopt(curl, CURLOPT_POST, 1)) == CURLE_OK &&
        (rc = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers)) ==
            CURLE_OK &&
        (rc = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data)) ==
            CURLE_OK) {
    } else {
      WARN(" curl init failed , code : %s [ %d ]", curl_easy_strerror(rc), rc);
      return false;
    }
    _putIndex >> [&](const string &recordJson) {
      CURLcode rc;
      if ((rc = curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                                 recordJson.c_str())) == CURLE_OK &&
          (rc = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                                 recordJson.length())) == CURLE_OK &&
          (rc = curl_easy_perform(curl)) == CURLE_OK) {
        DEBUG(" curl call ok ");
      } else {
        WARN(" curl call fails , code : %s [ %d ]", curl_easy_strerror(rc), rc);
      }
    };
    return true;
  }
  void deInit() { curl_easy_cleanup(curl); }
  Sink<string> &putIndex() { return _putIndex; }
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

void replyToMap(JsonObject &json, redisReply *reply) {
  if (reply->type != REDIS_REPLY_ARRAY) return;
  for (int i = 0; i < reply->elements; i += 2) {
    string key = reply->element[i]->str;
    redisReply *el = reply->element[i + 1];
    if (el->type == REDIS_REPLY_STRING)
      json[key] = el->str;
    else if (el->type == REDIS_REPLY_INTEGER) {
      json[key] = el->integer;
    } else {
      WARN(" cannot add element ");
    }
  }
}

//==========================================================================
int main(int argc, char **argv) {
  Config config = loadConfig(argc, argv);
  Thread workerThread("worker");
  Thread elasticThread("elastic");

  ElasticIndex indexLogs(elasticThread, config["elastic"], "logs");
  ElasticIndex indexMetrics(elasticThread, config["elastic"], "metrics");
  BrokerRedis broker(workerThread, config["broker"]);

  TimerSource pubTimer(workerThread, 2000, true, "pubTimer");
  CborSerializer cborSerializer(1024);
  CborDeserializer cborDeserializer(1024);
  TimerSource ticker(workerThread, 3000, true, "ticker");

  broker.init();
  indexLogs.init();
  indexMetrics.init();
  int rc = broker.connect("brain");
  broker.subscribe("*");

  // subscribe to all numbers and send to elastic

  broker.incoming() >> [&](const PubMsg &msg) {
    StaticJsonDocument<1024> jsonDoc;
    JsonObject json = jsonDoc.to<JsonObject>();
    vector<string> parts = split(msg.topic, '/');
    json["node"] = parts[1];
    json["object"] = parts[2];
    json["property"] = parts[3];
    json["timestamp"] = Sys::millis();
    double doubleValue;
    int64_t int64Value;
    string request;
    if (cborDeserializer.fromBytes(msg.payload)
            .begin()
            .get(doubleValue)
            .success()) {
      json["value"] = doubleValue;
    } else if (cborDeserializer.fromBytes(msg.payload)
                   .begin()
                   .get(int64Value)
                   .success()) {
      json["value"] = int64Value;
    } else {
      jsonDoc.clear();
      return;
    }
    serializeJson(jsonDoc, request);
    DEBUG("request :%s", request.c_str());
    indexMetrics.putIndex().on(request);
    jsonDoc.clear();
  };
  TimeoutFlow<uint64_t> fl(workerThread, 2000);

  workerThread.start();
  elasticThread.start();

  // send logs stream
  StaticJsonDocument<10240> jsonDoc;
  JsonObject json;
  while (true) {
    redisReply *reply;
    reply = broker.xread("logs");
    if (reply != 0 && reply->type == REDIS_REPLY_ARRAY) {
      INFO("reply :%s", broker.replyToString(reply).c_str());
      for (int i = 0; i < reply->elements; i++) {
        redisReply *streamElement = reply->element[i];
        if (streamElement->type == REDIS_REPLY_ARRAY) {
          string streamKey = streamElement->element[0]->str;
          redisReply *records = streamElement->element[1];
          for (int j = 0; j < records->elements; j++) {
            redisReply *record = records->element[j];
            string timestamp = record->element[0]->str;
            json = jsonDoc.to<JsonObject>();
            replyToMap(json, record->element[1]);
            json["stream"] = streamKey;
            json["timestamp"] = Sys::millis();
            string request;
            serializeJson(jsonDoc, request);
            DEBUG("request :%s", request.c_str());
            indexLogs.putIndex().on(request);
            jsonDoc.clear();
          }
        }
      }
    }
  };

  broker.disconnect();
  indexLogs.deInit();
  indexMetrics.deInit();
}
