#ifndef _BROKER_PROTOCOL_H_
#define _BROKER_PROTOCOL_H_
#include <context.h>

typedef enum
{
  B_PUBLISH,   // RXD,TXD id , Bytes value
  B_SUBSCRIBE, // TXD id, string, qos,
  B_UNSUBSCRIBE,
  B_NODE,
  B_LOG
} MsgType;

#define TOPIC_SIZE 40
#define VALUE_SIZE 256
#define CLIENTID_SIZE 20
#define MTU_SIZE 300

struct MsgBase
{
  int msgType;
  template <typename Reflector>
  Reflector &reflect(Reflector &r)
  {
    r.begin().member(msgType, "msgType", "type of polymorphic message").end();
    //   INFO(" looking for int %d ", msgType);
    return r;
  }
};
/*
struct MsgConnect
{
  ClientId clientId;
  static const int TYPE = B_CONNECT;
  template <typename Reflector>
  Reflector &reflect(Reflector &r)
  {
    return r.begin()
        .member(TYPE, "msgType", "MsgConnect")
        .member(clientId, "clientId", "client identification")
        .end();
  }
};

struct MsgDisconnect
{
  static const int TYPE = B_DISCONNECT;

  template <typename Reflector>
  Reflector &reflect(Reflector &r)
  {
    return r.begin().member(TYPE, "msgType", "MsgDisconnect").end();
  }
};*/

struct MsgPublish
{
  int id;
  Bytes value;

  static const int TYPE = B_PUBLISH;

  template <typename Reflector>
  Reflector &reflect(Reflector &r)
  {
    return r.begin()
        .member(TYPE, "msgType", "PublishMsg")
        .member(id, "id", "resource id")
        .member(value, "value", "data published")
        .end();
  }
};
/*
struct MsgPublisher
{
  int id;
  TopicName topic;

  static const int TYPE = B_PUBLISHER;

  template <typename Reflector>
  Reflector &reflect(Reflector &r)
  {
    return r.begin()
        .member(TYPE, "msgType", "MsgPublisher")
        .member(id, "id", "resource id")
        .member(topic, "topic", "publish topic")
        .end();
  }
};


struct MsgSubscriber
{
  int id;
  TopicName topic;

  static const int TYPE = B_SUBSCRIBER;

  template <typename Reflector>
  Reflector &reflect(Reflector &r)
  {
    return r.begin()
        .member(TYPE, "msgType", "MsgSubscriber")
        .member(id, "id", "resource id")
        .member(topic, "topic", "publish topic")
        .end();
  }
};
*/
#endif