#include <CborDump.h>
#include <ReflectFromCbor.h>
#include <ReflectToCbor.h>
#include <ReflectToDisplay.h>
#include <assert.h>
#include <broker_protocol.h>
#include <hiredis.h>
#include <limero.h>
#include <log.h>

// using namespace sw::redis;

#include <iostream>
using namespace std;
const int MsgPublish::TYPE;


LogS logger;

#include <gtest/gtest.h>
#define KEY_COUNT 5

#define panicAbort(fmt, ...)                                                   \
  do {                                                                         \
    fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__,          \
            __VA_ARGS__);                                                      \
    exit(-1);                                                                  \
  } while (0)

static void assertReplyAndFree(redisContext *context, redisReply *reply,
                               int type) {
  if (reply == NULL)
    panicAbort("NULL reply from server (error: %s)", context->errstr);

  if (reply->type != type) {
    if (reply->type == REDIS_REPLY_ERROR)
      fprintf(stderr, "Redis Error: %s\n", reply->str);

    panicAbort("Expected reply type %d but got type %d", type, reply->type);
  }

  freeReplyObject(reply);
}

void privdata_dtor(void *privdata) {
  unsigned int *icount = (unsigned int *)privdata;
  printf("privdata_dtor():  In context privdata dtor (invalidations: %u)\n",
         *icount);
}
/* Switch to the RESP3 protocol and enable client tracking */
static void enableClientTracking(redisContext *c) {
  redisReply *reply = (redisReply *)redisCommand(c, "HELLO 3");
  EXPECT_FALSE(reply == NULL || c->err);

  if (reply->type != REDIS_REPLY_MAP) {
    fprintf(stderr, "Error: Can't send HELLO 3 command.  Are you sure you're ");
    fprintf(stderr, "connected to redis-server >= 6.0.0?\nRedis error: %s\n",
            reply->type == REDIS_REPLY_ERROR ? reply->str : "(unknown)");
    exit(-1);
  }

  freeReplyObject(reply);

  /* Enable client tracking */
  reply = (redisReply *)redisCommand(c, "CLIENT TRACKING ON");
  assertReplyAndFree(c, reply, REDIS_REPLY_STATUS);
}
// Demonstrate some basic assertions.
TEST(RedisConnect, BasicAssertions) {
  unsigned int j, invalidations = 0;

  redisContext *c;
  redisReply *reply;
  const char *hostname = "127.0.0.1";
  int port = 6379;

  redisOptions o = {0};
  REDIS_OPTIONS_SET_TCP(&o, hostname, port);
  REDIS_OPTIONS_SET_PRIVDATA(&o, &invalidations, privdata_dtor);
  c = redisConnectWithOptions(&o);
  EXPECT_FALSE(c == NULL || c->err);
  enableClientTracking(c);
  /* Set some keys and then read them back.  Once we do that, Redis will deliver
   * invalidation push messages whenever the key is modified */
  for (j = 0; j < KEY_COUNT; j++) {
    reply = (redisReply *)redisCommand(c, "SET key:%d initial:%d", j, j);
    assertReplyAndFree(c, reply, REDIS_REPLY_STATUS);

    reply = (redisReply *)redisCommand(c, "GET key:%d", j);
    assertReplyAndFree(c, reply, REDIS_REPLY_STRING);
  }

  /* Trigger invalidation messages by updating keys we just read */
  for (j = 0; j < KEY_COUNT; j++) {
    printf("            main(): SET key:%d update:%d\n", j, j);
    reply = (redisReply *)redisCommand(c, "SET key:%d update:%d", j, j);
    assertReplyAndFree(c, reply, REDIS_REPLY_STATUS);
    printf("            main(): SET REPLY OK\n");
  }

  printf("\nTotal detected invalidations: %d, expected: %d\n", invalidations,
         KEY_COUNT);
  redisFree(c);
}
//=============================================================================


class Redis {
public:
  Redis(redisContext *c) : _c(c){};
  redisContext *_c;
  void readInvoke() {
    redisReply *reply;
    if (redisGetReply(_c, (void **)&reply) == REDIS_OK) {
      freeReplyObject(reply);
    };
  }
  void errorInvoke();
  int connect() {
    _c = redisConnect("127.0.0.1", 6379);
    assert(_c != 0);
  }
};

Bytes readFromFd(int _fd) {
  Bytes out;
  char buffer[1024];
  int rc;
  while (true) {
    rc = read(_fd, buffer, sizeof(buffer));
    if (rc > 0) {
      DEBUG("read() = %d bytes", rc);
      for (int i = 0; i < rc; i++)
        out.push_back(buffer[i]);
    } else if (rc < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return out;
    } else { // no data
      return out;
    }
  }
}

void showReply(redisReply *reply) {
  if (reply->type == REDIS_REPLY_ARRAY) {
    for (int j = 0; j < reply->elements; j++) {
      LOGI << j << ":" << reply->element[j]->str << LEND;
    }
  } else if (reply->type == REDIS_REPLY_INTEGER) {
    LOGI << " REDIS_REPLY_INTEGER : " << reply->integer << LEND;
  } else if (reply->type == REDIS_REPLY_STATUS) {
    LOGI << " REDIS_REPLY_STATUS : " << reply->integer << " str " << reply->str
         << LEND;
  } else if (reply->type == REDIS_REPLY_STRING) {
    LOGI << " REDIS_REPLY_STRING   : " << reply->integer << " str "
         << reply->str << LEND;
  } else {
    LOGI << " Type : " << reply->type << LEND;
  }
}

TEST(HiRedis, BasicAssertions) {
  redisContext *c = redisConnectNonBlock("127.0.0.1", 6379);
  redisReader *reader = redisReaderCreate();
  Thread listener("listener");
  TimerSource ticker(listener, 5000, true, "ticker");
  ticker >> [&](const TimerMsg &) { INFO("tick."); };
  INFO(" socket fd %d add read invoker ", c->fd);
  listener.addReadInvoker(c->fd, [&](int fd) {
    INFO(" ENTER read .. %d ", fd);
    redisReply *reply;
    Bytes buffer = readFromFd(c->fd);
    if (buffer.size()) {
      int rc =
          redisReaderFeed(reader, (const char *)buffer.data(), buffer.size());
      INFO("%d %s", rc, charDump(buffer).c_str());
      if (redisReaderGetReply(reader, (void **)&reply) == 0) {
        if (reply != 0) {
          showReply(reply);
          freeReplyObject(reply);
        } else {
          INFO("empty ");
        }
      }
    }

    /*    while (redisGetReply(c, (void **)&reply) == REDIS_OK && reply != 0) {
          freeReplyObject(reply);
        }*/
    INFO(" EXIT read ");
  });
  listener.start();
  int wdone = 0;
  redisCommand(c, "GET foo");
  redisCommand(c, "SET foo ABCD");
  redisCommand(c, "PSUBSCRIBE XX");
  redisCommand(c, "PSUBSCRIBE YY");
  redisCommand(c, "PING");

  redisBufferWrite(c, &wdone);

  /*  redisReply *reply;
  if (redisGetReply(c, (void **)&reply) == REDIS_OK) {
        INFO(" got reply 2");
        freeReplyObject(reply);
      };*/
  sleep(10);
  redisFree(c);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}