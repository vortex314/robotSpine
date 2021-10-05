#include "log.h"
#include "limero.h"


#include <asm-generic/ioctls.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

NanoStats stats;
/*
 _____ _                        _
|_   _| |__  _ __ ___  __ _  __| |
  | | | '_ \| '__/ _ \/ _` |/ _` |
  | | | | | | | |  __/ (_| | (_| |
  |_| |_| |_|_|  \___|\__,_|\__,_|
*/
int Thread::_id = 0;

void Thread::buildFdSet() {
  FD_ZERO(&_rfds);
  FD_ZERO(&_wfds);
  FD_ZERO(&_efds);
  _maxFd = _readPipe;
  FD_SET(_readPipe, &_rfds);
  FD_SET(_readPipe, &_efds);
  for (const auto &myPair : _readInvokers) {
    int fd = myPair.first;
    FD_SET(fd, &_rfds);
    if (fd > _maxFd) _maxFd = fd;
  }
  for (const auto &myPair : _errorInvokers) {
    int fd = myPair.first;
    FD_SET(fd, &_efds);
    if (fd > _maxFd) _maxFd = fd;
  }
  _maxFd += 1;
}

void Thread::addReadInvoker(int fd, std::function<void(int)> f) {
  _readInvokers.emplace(fd, f);
  buildFdSet();
}

void Thread::addErrorInvoker(int fd, std::function<void(int)> f) {
  _errorInvokers.emplace(fd, f);
  buildFdSet();
}

void Thread::deleteInvoker(int fd) {
  if (_readInvokers.find(fd) != _readInvokers.end()) _readInvokers.erase(fd);
  if (_errorInvokers.find(fd) != _errorInvokers.end()) _errorInvokers.erase(fd);
  buildFdSet();
}

int Thread::waitInvoker(uint32_t timeout) {
  fd_set rfds = _rfds;
  fd_set wfds = _wfds;
  fd_set efds = _efds;
  Invoker *invoker;

  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout * 1000) % 1000000;

  int rc = select(_maxFd, &rfds, 0, &efds, &tv);

  if (rc < 0) {
    WARN(" select() : error : %s (%d)", strerror(errno), errno);
    return rc;
  } else if (rc > 0) {  // one of the fd was set

    if (FD_ISSET(_readPipe, &rfds)) {
      ::read(_readPipe, &invoker, sizeof(Invoker *));  // read 1 event
      invoker->invoke();
    }
    for (const auto &myPair : _readInvokers) {
      int fd = myPair.first;
      std::function<void(int)> f = myPair.second;
      if (FD_ISSET(fd, &rfds)) f(fd);
    }
    for (const auto &myPair : _errorInvokers) {
      int fd = myPair.first;
      std::function<void(int)> f = myPair.second;
      if (FD_ISSET(fd, &efds)) f(fd);
    }
    if (FD_ISSET(_readPipe, &efds)) {
      WARN("pipe  error : %s (%d)", strerror(errno), errno);
      return ECOMM;
    }
    return 0;
  } else {
    DEBUG(" timeout %llu", Sys::millis());
    return ETIMEDOUT;
  }
  return ECOMM;
}

Thread::Thread(const char *name) : Named(name) { createQueue(); }

void Thread::createQueue() {
  int rc = pipe(_pipeFd);
  _writePipe = _pipeFd[1];
  _readPipe = _pipeFd[0];
  if (rc < 0) WARN("Queue creation failed %d %s ", errno, strerror(errno));
  if (fcntl(_writePipe, F_SETFL, O_NONBLOCK) < 0) {
    WARN("Failed to set pipe blocking mode: %s (%d)", strerror(errno), errno);
  }
}

void Thread::addTimer(TimerSource *ts) { _timers.push_back(ts); }

void SetThreadName(std::thread *thread, const char *threadName) {
  auto handle = thread->native_handle();
  pthread_setname_np(handle, threadName);
}

void Thread::start() {
  INFO("Thread %s started", name());
  std::thread *thr = new std::thread(&Thread::run, this);
  SetThreadName(thr, name());
}

int Thread::enqueue(Invoker *invoker) {
  //  INFO("Thread '%s' >>> '%lX", _name.c_str(), invoker);
  if (_writePipe)
    if (write(_writePipe, (const char *)&invoker, sizeof(Invoker *)) == -1) {
      stats.threadQueueOverflow++;
      WARN("Thread '%s' queue overflow [%X]", name(), invoker);
      return ENOBUFS;
    }
  return 0;
}
int Thread::enqueueFromIsr(Invoker *invoker) { return enqueue(invoker); }

void Thread::run() {
  INFO("Thread '%s' started ", name());
  buildFdSet();
  while (true) {
    uint64_t now = Sys::millis();
    uint64_t expTime = now + 5000;
    TimerSource *expiredTimer = 0;
    // find next expired timer if any within 5 sec
    for (auto timer : _timers) {
      if (timer->expireTime() < expTime) {
        expTime = timer->expireTime();
        expiredTimer = timer;
      }
    }
    int32_t waitTime = (expTime - now);
    if (waitTime > 0) {
      int rc = waitInvoker(waitTime);
      if (rc == ETIMEDOUT && expiredTimer) {
        expiredTimer->request();
      }
    } else {
      if (expiredTimer) {
        if (-waitTime > 100)
          INFO("Timer[%X] already expired by %u msec on thread '%s'.",
               expiredTimer, -waitTime, name());
        expiredTimer->request();
      }
    }
  }
}
