#pragma once
#include <ArduinoJson.h>
#include <Mm1Packet.h>
namespace quickSend {
constexpr unsigned Count=10,TextBytes=160;
struct State {uint64_t to=meshmesh::Broadcast;bool gps=false;char text[Count][TextBytes+1]={};};
extern State state;
void begin();String json();String command(JsonObjectConst values);
String send(unsigned index,uint64_t to=0);String targetName();bool targetValid(uint64_t to);
unsigned targetCount();uint64_t target(unsigned index);
}
