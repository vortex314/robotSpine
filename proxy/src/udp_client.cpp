#include <ReflectToCbor.h>
#include <CborDump.h>
#include <Udp.h>
#include <broker_protocol.h>
#include <log.h>
LogS logger;
const int MsgPublish::TYPE;


int main(int argc, char **argv) {
/*  Udp udp;
  ReflectToCbor toCbor(1024);

  udp.port(12001);
  udp.init();
  MsgConnect msgConnect = {"udp-client"};
  Bytes msg = msgConnect.reflect(toCbor).toBytes();
  UdpMsg udpMsg;
  udpMsg.dstIpString("127.0.0.1");
  udpMsg.dstPort(12000);
  udpMsg.message = msg;
  udp.send(udpMsg);
  LOGI << cborDump(udpMsg.message) << LEND;
  Sys::delay(1000);*/
}