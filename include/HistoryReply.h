#pragma once
#include "MeshRadio.h"
#include <memory>
// An immutable snapshot in small allocations. A caller advances only bytes actually sent.
class HistoryReply {
 static constexpr unsigned BlockSize=8;
 struct Block {ChatMessage messages[BlockSize];};
 std::unique_ptr<Block> blocks[8];
 String row;unsigned count=0,index=0,offset=0;
 enum Phase {Open,Record,Comma,Close,Newline,Done} phase=Done;
 bool valid=true;
 public:
 bool begin();
 const uint8_t* peek(size_t& size);
 void advance(size_t size);
 bool finished() const{return phase==Done;}
 bool okay() const{return valid;}
};
