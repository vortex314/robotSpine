
#include <BrokerRedis.h>
#include <CborDump.h>

void BrokerRedis::onMessage(redisContext *c, void *reply, void *me)
{
  BrokerRedis *pBroker = (BrokerRedis *)me;
  if (reply == NULL)
    return;
  redisReply *r = (redisReply *)reply;
  if (r->type == REDIS_REPLY_ARRAY)
  {
    if (strcmp(r->element[0]->str, "pmessage") == 0)
    {
      string topic = r->element[2]->str;
      pBroker->_incoming.on(
          {topic,
           Bytes(r->element[3]->str, r->element[3]->str + r->element[3]->len)});
    }
    else
    {
      LOGW << "unexpected array " << r->element[0]->str << LEND;
    }
  }
  else
  {
    LOGW << " unexpected reply " << LEND;
  }
}

bool isFailedReply(redisReply *reply)
{
  return reply == 0 || reply->type == REDIS_REPLY_ERROR;
}

string BrokerRedis::replyToString(void *r)
{
  if (r == 0)
  {
    return "Reply failed ";
  }
  redisReply *reply = (redisReply *)r;

  switch (reply->type)
  {
  case REDIS_REPLY_ARRAY:
  {
    string result = "[";
    for (int j = 0; j < reply->elements; j++)
    {
      result += replyToString(reply->element[j]);
      result += ",";
    }
    result += "]";
    return result;
  }
  case REDIS_REPLY_INTEGER:
  {
    return to_string(reply->integer);
  }
  case REDIS_REPLY_STRING:
    return stringFormat("'%s'", reply->str);
  case REDIS_REPLY_STATUS:
    return stringFormat("(status) %s", reply->str);
  case REDIS_REPLY_NIL:
    return "(nill)";
  case REDIS_REPLY_ERROR:
    return stringFormat(" (error) %s", reply->str);
  default:
    return stringFormat("unexpected redisReply type : %d", reply->type);
  }
  return "XXX";
}

BrokerRedis::BrokerRedis(Thread &thread, Config cfg)
    : BrokerBase(thread, cfg),
      _thread(thread),
      _reconnectTimer(thread, 3000, true, "reconnectTimer")
{
  _hostname = cfg["host"] | "localhost";
  _port = cfg["port"] | 6379;

  _reconnectHandler.async(thread);
  _reconnectHandler >> [&](const bool &)
  { reconnect(); };

  connected >> [](const bool &connected)
  {
    LOGI << "Connection state : " << (connected ? "connected" : "disconnected")
         << LEND;
  };
  connected = false;
  _reconnectTimer >> [&](const TimerMsg &)
  {
    if (!connected())
    {
      if (connect(_node) == 0)
        subscribeAll();
    }
  };
  _incoming >> [&](const PubMsg &msg)
  {
    DEBUG("Redis RXD %s %s ", msg.topic.c_str(),
          msg.payload.size() < 100
              ? cborDump(msg.payload).c_str()
              : stringFormat("size : [%d]", msg.payload.size()).c_str());
  };
  _outgoing >>
      [&](const PubMsg &msg)
  {
    DEBUG("publish %s %s ", msg.topic.c_str(), cborDump(msg.payload).c_str());
    publish(msg.topic, msg.payload);
  };
}

void free_privdata(void *pvdata) {}

int BrokerRedis::init() { return 0; }

int BrokerRedis::connect(string node)
{
  _node = node;
  _dstPrefix = "dst/";
  _dstPrefix += _node + "/";
  _srcPrefix = "src/";
  _srcPrefix += _node + "/";
  if (connected())
  {
    LOGI << " Connecting but already connected." << LEND;
    connected = true;
    return 0;
  }
  redisOptions options = {0};
  REDIS_OPTIONS_SET_TCP(&options, _hostname.c_str(), _port);
  options.connect_timeout = new timeval{3, 0}; // 3 sec
  options.command_timeout = new timeval{3, 0}; // 3 sec
  REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);

  LOGI << "Connecting to Redis " << _hostname << ":" << _port << LEND;
  _subscribeContext = redisConnectWithOptions(&options);
  if (_subscribeContext == NULL || _subscribeContext->err)
  {
    INFO(" Connection %s:%d failed", _hostname.c_str(), _port);
    return ENOTCONN;
  }
  _thread.addErrorInvoker(_subscribeContext->fd, [&](int)
                          {
                            INFO(" error occured ");
                            _reconnectHandler.on(true);
                          });
  _thread.addReadInvoker(_subscribeContext->fd, [&](int)
                         {
                           redisReply *reply;

                           redisBufferRead(_subscribeContext);
                           while (redisGetReplyFromReader(_subscribeContext, (void**)&reply) == REDIS_OK && reply )
                           {
                             if (reply->type == REDIS_REPLY_ARRAY &&
                                 strcmp(reply->element[0]->str, "pmessage") == 0)
                             {
                               onMessage(_subscribeContext, reply, this);
                             }
                             else
                             {
                               INFO(" reply not handled %s ",replyToString(reply).c_str());
                             }
                             freeReplyObject(reply);
                           }

                           /*                       int rc = redisGetReply(_subscribeContext, (void **)&reply);
                           if (rc == 0)
                           {
                             if (reply->type == REDIS_REPLY_ARRAY &&
                                 strcmp(reply->element[0]->str, "pmessage") == 0)
                             {
                               onMessage(_subscribeContext, reply, this);
                             }
                             else
                             {
                               INFO(" reply not handled ");
                             }

                             freeReplyObject(reply);
                           }
                           else
                           {
                             INFO(" reply not found ");
                             _reconnectHandler.on(true);
                           }*/
                         });
  _publishContext = redisConnectWithOptions(&options);
  if (_publishContext == NULL || _publishContext->err)
  {
    INFO(" Connection %s:%d failed", _hostname.c_str(), _port);
    return ENOTCONN;
  }
  connected = true;
  _publishContext->privdata = this;
  _subscribeContext->privdata = this;
  return 0;
}

int BrokerRedis::subscribeAll()
{
  INFO("subscribeAll again");
  if (connected())
  {
    for (auto sub : _subscriptions)
    {
      INFO(" subscribing %s", sub.c_str());
      subscribe(sub);
    }
  }
  return 0;
}

int BrokerRedis::reconnect()
{
  int rc;
  disconnect();
  if ((rc = connect(_node)) == 0)
  {
    subscribeAll();
    return 0;
  }
  else
    return rc;
}

int BrokerRedis::disconnect()
{
  LOGI << (" disconnecting.") << LEND;
  if (!connected())
    return 0;
  _thread.deleteInvoker(_subscribeContext->fd);
  redisFree(_publishContext);
  redisFree(_subscribeContext);
  connected = false;
  return 0;
}

int BrokerRedis::subscribe(string pattern)
{
  INFO(" REDIS psubscribe %s", pattern.c_str());
  _subscriptions.insert(pattern);
  string cmd = stringFormat("PSUBSCRIBE %s", pattern.c_str());
  redisReply *r = (redisReply *)redisCommand(_subscribeContext, cmd.c_str());
  if (r)
  {
    INFO("%s OK.", cmd.c_str());
    freeReplyObject(r);
    return 0;
  }
  else
  {
    WARN("%s failed.", cmd.c_str());
    return EIO;
  }
}

int BrokerRedis::unSubscribe(string pattern)
{
  auto it = _subscriptions.find(pattern);
  if (it != _subscriptions.end())
  {
    _subscriptions.erase(pattern);
    redisReply *r = (redisReply *)redisCommand(
        _subscribeContext, "PUNSUBSCRIBE %s", pattern);
    if (r)
    {
      INFO(" PUNSUBSCRIBE : %s created.", pattern.c_str());
      _subscriptions.erase(pattern);
      freeReplyObject(r);
    }
  }
  return 0;
}

int BrokerRedis::publish(string topic, const Bytes &bs)
{
  if (!connected())
    return ENOTCONN;
  redisReply *r = (redisReply *)redisCommand(
      _publishContext, "PUBLISH %s %b", topic.c_str(), bs.data(), bs.size());
  if (isFailedReply(r))
  {
    WARN("Redis PUBLISH '%s' failed  : %s ", topic.c_str(), replyToString(r).c_str());
    _reconnectHandler.on(true);
  }
  else
  {
    // showReply(r);
    freeReplyObject(r);
    DEBUG("Redis PUBLISH %s : [%d] ", topic.c_str(), bs.size());
  }
  return 0;
}

int BrokerRedis::command(const char *format, ...)
{
  if (!connected())
    return ENOTCONN;
  va_list ap;
  va_start(ap, format);
  redisReply *reply = (redisReply *)redisvCommand(_publishContext, format, ap);
  va_end(ap);
  if (isFailedReply(reply))
  {
    INFO(" command failed : '%s' : %s ", format, replyToString(reply).c_str());
    if (reply)
      freeReplyObject(reply);
    _reconnectHandler.on(true);
    return EINVAL;
  }
  else
  {
    DEBUG(" command success : %s : %s ", format, replyToString(reply).c_str());
    if (reply)
      freeReplyObject(reply);
    return 0;
  }
}

int BrokerRedis::request(string cmd, std::function<void(redisReply *)> func)
{
  if (!connected())
    return ENOTCONN;

  redisReply *reply = (redisReply *)redisCommand(_publishContext, cmd.c_str());
  if (reply)
  {
    func(reply);
    freeReplyObject(reply);
    return 0;
  }
  INFO("command : %s failed. ", cmd.c_str());
  _reconnectHandler.on(true);
  return EINVAL;
}

#include <regex>
bool BrokerRedis::match(string pattern, string topic)
{
  std::regex patternRegex(pattern);
  return std::regex_match(topic, patternRegex);
}

redisReply *BrokerRedis::xread(string key)
{
  if (!connected())
    return 0;
  return (redisReply *)redisCommand(_publishContext,
                                    "XREAD BLOCK 0 STREAMS %s $", key.c_str());
}