#include <limero.h>
#include <context.h>
#include <log.h>
NanoStats stats;

/*
 _____ _                        _
|_   _| |__  _ __ ___  __ _  __| |
  | | | '_ \| '__/ _ \/ _` |/ _` |
  | | | | | | | |  __/ (_| | (_| |
  |_| |_| |_|_|  \___|\__,_|\__,_|
*/
int Thread::_id = 0;

Thread::Thread(const char *name) : Named(name), _workQueue(10){};

void Thread::addTimer(TimerSource *ts)
{
  _timers.push_back(ts);
}

void Thread::createQueue() {}

void Thread::start() {}

int Thread::enqueue(Invoker *invoker)
{
  //	LOGI("Thread '%s' >>> '%s'",name(),symbols(invoker));
  _workQueue.push(invoker);
  return 0;
};

void Thread::run()
{
  LOGI << "Thread '" << name() << "' started ";
  while (true)
    loop();
}

void Thread::loop()
{
  uint64_t now = Sys::millis();
  uint64_t expTime = now + 5000;
  TimerSource *expiredTimer = 0;
  // find next expired timer if any within 5 sec
  for (auto timer : _timers)
  {
    if (timer->expireTime() < expTime)
    {
      expTime = timer->expireTime();
      expiredTimer = timer;
    }
  }
  int32_t waitTime =
      (expTime - now); // ESP_OPEN_RTOS seems to double sleep time ?

  if (waitTime > 0)
  {
    Invoker *prq;
    if (_workQueue.pop(prq) == true)
    {
      uint64_t start = Sys::millis();
      prq->invoke();
      uint32_t delta = Sys::millis() - start;
      if (delta > 20)
        LOGW << "Invoker [" << (unsigned int)prq << "] slow " << delta << " msec invoker on thread " << name();
    }
  }
  else
  {
    if (expiredTimer && expiredTimer->expireTime() < Sys::millis())
    {
      if (-waitTime > 100)
        LOGW << "Timer[" << (unsigned int)expiredTimer << " already expired by " << -waitTime << "msec on thread " << name();
      uint64_t start = Sys::millis();
      expiredTimer->request();
      uint32_t deltaExec = Sys::millis() - start;
      if (deltaExec > 20)
        LOGW << "Timer ["<<(unsigned int)expiredTimer<<"] request slow "<<deltaExec<<" msec on thread "<<name();
    }
  }
}
