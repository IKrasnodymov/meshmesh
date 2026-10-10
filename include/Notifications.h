#pragma once
#include "MeshRadio.h"
namespace notifications {
enum Mode:uint8_t {Off,All,Mentions,Inherit};
enum Kind:uint8_t {Incoming,Transmitted,Confirmed,Unconfirmed,TxError,Repeat};
struct Event {Kind kind=Incoming;ChatMessage message{};bool mention=false;};
void incoming(const ChatMessage& message);void emit(Kind kind,const ChatMessage& message);
void repeatPacket(uint32_t packet);
void tick();bool take(Event& event);uint32_t dropped();
bool mentioned(const char* text,const char* name,const char* words);
bool enabled(uint8_t mode,bool mention);
uint8_t mode(const ChatMessage& message,uint8_t global,unsigned effect); // 0 LED, 1 screen, 2 popup
}
