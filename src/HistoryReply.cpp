#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "HistoryReply.h"
#include "HistoryJson.h"
#include <new>
bool HistoryReply::begin(){
 for(auto& block:blocks)block.reset();row="";count=meshRadio.historyCount;index=offset=0;valid=true;phase=Done;
 if(count>64)return false;
 for(unsigned i=0;i<count;i+=BlockSize){
  blocks[i/BlockSize].reset(new(std::nothrow) Block);
  if(!blocks[i/BlockSize]){for(auto& block:blocks)block.reset();return false;}
  for(unsigned j=0;j<BlockSize&&i+j<count;j++)blocks[i/BlockSize]->messages[j]=meshRadio.history[i+j];
 }
 phase=Open;return true;
}
const uint8_t* HistoryReply::peek(size_t& size){
 static const uint8_t open='[',comma=',',close=']',newline='\n';size=1;
 switch(phase){
 case Open:return &open;case Comma:return &comma;case Close:return &close;case Newline:return &newline;
 case Record:
  if(!row.length()){
   StaticJsonDocument<1024>d;historyJsonRecord(d,blocks[index/BlockSize]->messages[index%BlockSize]);
   size_t bytes=measureJson(d);if(d.overflowed()||!row.reserve(bytes)){valid=false;phase=Done;size=0;return nullptr;}
   serializeJson(d,row);if(row.length()!=bytes){valid=false;phase=Done;size=0;return nullptr;}
  }
  size=row.length()-offset;return reinterpret_cast<const uint8_t*>(row.c_str()+offset);
 case Done:size=0;return nullptr;
 }
 size=0;return nullptr;
}
void HistoryReply::advance(size_t size){
 if(!size||phase==Done)return;
 if(phase==Record){offset+=size;if(offset<row.length())return;row="";offset=0;index++;phase=index<count?Comma:Close;return;}
 switch(phase){case Open:phase=count?Record:Close;break;case Comma:phase=Record;break;case Close:phase=Newline;break;case Newline:phase=Done;break;default:break;}
}
