#ifndef _SESSION_ABSTRACT_H_
#define _SESSION_ABSTRACT_H_
#include <limero.h>

class SessionError;

class SessionAbstract : public Actor, public Invoker {
 public:
  SessionAbstract(Thread &thread, Config config) : Actor(thread){};
  virtual bool init() = 0;
  virtual bool connect() = 0;
  virtual bool disconnect() = 0;
  virtual Source<Bytes> &incoming() = 0;
  virtual Sink<Bytes> &outgoing() = 0;
  virtual Source<bool> &connected() = 0;
  virtual Source<Bytes> &logs() = 0;
  virtual void onError() = 0;
  virtual int fd() = 0;
  virtual void invoke() = 0;
};

class SessionError : public Invoker {
  SessionAbstract &_session;

 public:
  SessionError(SessionAbstract &session) : _session(session){};
  void invoke() { _session.onError(); }
};
#endif