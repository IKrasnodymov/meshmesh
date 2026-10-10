#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "MeshRadio.h"
#include "Hardware.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#if defined(MM_UI_PREVIEW)
using HistoryFs=PreviewLittleFs;
static constexpr size_t historySegment=12*1024;
static HistoryFs* historyFs(){return hardware.fsOk?&LittleFS:nullptr;}
#else
#if defined(MM_NRF52)
using HistoryFs=MeshFS; // no SD card; a smaller log in the 100 KB internal storage
static constexpr size_t historySegment=12*1024;
static HistoryFs* historyFs(){return hardware.fsOk?&LittleFS:nullptr;}
#else
#include <SD.h>
using HistoryFs=fs::FS;
static constexpr size_t historySegment=256*1024;
static HistoryFs* historyFs(){return hardware.sdOk?static_cast<fs::FS*>(&SD):hardware.fsOk?static_cast<fs::FS*>(&LittleFS):nullptr;}
#endif
#endif
namespace {
bool snapshotReady=false;
bool commitSnapshot(HistoryFs* fs){
 // Keep a recoverable full file until the new one has its final name.
 if(fs->exists("/meshmesh/history.jsonl")){
  if(fs->exists("/meshmesh/history.previous.jsonl")&&!fs->remove("/meshmesh/history.previous.jsonl"))return false;
  if(!fs->rename("/meshmesh/history.jsonl","/meshmesh/history.previous.jsonl"))return false;
 }
 if(!fs->rename("/meshmesh/history.next.jsonl","/meshmesh/history.jsonl")){
  if(!fs->exists("/meshmesh/history.jsonl")&&fs->exists("/meshmesh/history.previous.jsonl"))fs->rename("/meshmesh/history.previous.jsonl","/meshmesh/history.jsonl");
  return false;
 }
 fs->remove("/meshmesh/history.previous.jsonl");snapshotReady=false;return true;
}
uint32_t rowChecksum(uint32_t crc,const String& row){for(unsigned i=0;i<row.length();i++){crc^=uint8_t(row[i]);crc*=16777619u;}crc^='\n';return crc*16777619u;}
String historyRow(const MeshRadio& radio,const ChatMessage& m){StaticJsonDocument<1024>d;d["protocol"]=m.protocol;d["source"]=radio.idText(m.source);d["destination"]=radio.idText(m.destination);d["session"]=m.session;d["id"]=m.id;d["time"]=m.timestamp;d["name"]=m.name;d["text"]=m.text;d["outgoing"]=m.outgoing;d["status"]=int(m.status);if(m.route){d["route"]=int(m.route);d["hops"]=m.hops;d["tries"]=m.tries;}MeshRadio::pathJson(d.as<JsonObject>(),m);String row;serializeJson(d,row);return row;}
bool completedHistory(HistoryFs* fs){File f=fs->open("/meshmesh/history.next.jsonl",FILE_READ);if(!f)return false;unsigned n=0;uint32_t crc=2166136261u;bool valid=false;while(f.available()){String row=f.readStringUntil('\n');StaticJsonDocument<1024>d;if(deserializeJson(d,row))break;if(d.containsKey("mm_commit")){valid=!f.available()&&d["mm_commit"].as<unsigned>()==n&&d["crc"].as<uint32_t>()==crc;break;}if(!d.containsKey("source")||++n>64)break;crc=rowChecksum(crc,row);}f.close();return valid;}
}
void MeshRadio::compactHistoryTick(){
#if defined(MM_NRF52)
 static File next(LittleFS);
#else
 static File next;
#endif
static uint32_t generation=0,crc=0;static unsigned index=0;HistoryFs* fs=historyFs();if(!fs||!compactRequested)return;
 if(snapshotReady){if(commitSnapshot(fs))compactRequested=false;return;}
 if(next&&generation!=historyGeneration){next.close();fs->remove("/meshmesh/history.next.jsonl");}
 if(!next){next=fs->open("/meshmesh/history.next.jsonl",FILE_WRITE);if(!next)return;generation=historyGeneration;index=0;crc=2166136261u;}
 if(index<historyCount){String row=historyRow(*this,history[index]);if(next.print(row)!=row.length()||next.write(uint8_t('\n'))!=1){next.close();fs->remove("/meshmesh/history.next.jsonl");return;}crc=rowChecksum(crc,row);index++;return;}
 String end="{\"mm_commit\":"+String(index)+",\"crc\":"+String(crc)+"}";bool written=next.print(end)==end.length()&&next.write(uint8_t('\n'))==1;next.close();if(!written){fs->remove("/meshmesh/history.next.jsonl");return;}
 snapshotReady=true;if(commitSnapshot(fs))compactRequested=false;
}
void MeshRadio::persist(const ChatMessage& m) {
  HistoryFs* fs=historyFs();if(!fs)return;
  if(snapshotReady&&!commitSnapshot(fs))return;
  File f=fs->open("/meshmesh/history.jsonl",FILE_APPEND);if(!f) {fs->mkdir("/meshmesh");f=fs->open("/meshmesh/history.jsonl",FILE_APPEND);}if(!f)return;
  // Bound the log: retain one previous segment; never touch other apps' files.
  if(f.size()>historySegment) {f.close();fs->remove("/meshmesh/history.previous.jsonl");fs->rename("/meshmesh/history.jsonl","/meshmesh/history.previous.jsonl");f=fs->open("/meshmesh/history.jsonl",FILE_APPEND);}
  String row=historyRow(*this,m);f.println(row);f.close();historyGeneration++;

}
void MeshRadio::restoreHistory() {
  HistoryFs* fs=historyFs();if(!fs)return;
  bool recovered=completedHistory(fs);
  for(const char* path:{"/meshmesh/history.previous.jsonl","/meshmesh/history.jsonl","/meshmesh/history.next.jsonl"}) {
    if(recovered?strcmp(path,"/meshmesh/history.next.jsonl")!=0:strcmp(path,"/meshmesh/history.next.jsonl")==0)continue;
    File f=fs->open(path,FILE_READ);if(!f)continue;
    while(f.available()) {
      String row=f.readStringUntil('\n');StaticJsonDocument<1024> d;if(deserializeJson(d,row)||!d.containsKey("source"))continue;
      ChatMessage m;m.protocol=d["protocol"]|1;m.source=strtoull(d["source"]|"0",nullptr,16);const char* dest=d["destination"]|"ALL";m.destination=strcmp(dest,"ALL")==0?meshmesh::Broadcast:strtoull(dest,nullptr,16);
      m.session=d["session"]|0u;m.id=d["id"]|0u;m.timestamp=d["time"]|0u; // unsigned: "|0" reads an id above 2^31 as 0
      m.outgoing=d["outgoing"]|false;m.status=ChatMessage::Status(constrain(d["status"]|0,0,4));
      m.route=ChatMessage::Route(constrain(d["route"]|0,0,2));m.hops=d["hops"]|255;m.tries=d["tries"]|0;
      strlcpy(m.name,d["name"]|"?",sizeof(m.name));strlcpy(m.text,d["text"]|"",sizeof(m.text));pathRead(d.as<JsonObjectConst>(),m);
      bool found=false;for(unsigned i=0;i<historyCount;i++) if(history[i].protocol==m.protocol && history[i].source==m.source && history[i].session==m.session && history[i].id==m.id) {history[i].status=m.status;if(m.route){history[i].route=m.route;history[i].hops=m.hops;history[i].tries=m.tries;}pathRead(d.as<JsonObjectConst>(),history[i]);if(m.timestamp>=1735689600)history[i].timestamp=m.timestamp;found=true;break;}
      if(!found)addMessage(m,false);
    }
    f.close();
  }
  if(recovered){snapshotReady=true;commitSnapshot(fs);}
  trimHistory();
  // Pending delivery belongs to the previous boot session and is not resumed.
  // Preserve confirmed deliveries and completed broadcasts; never invent an ACK.
  for(unsigned i=0;i<historyCount;i++) {
    auto& m=history[i];
    if(m.outgoing && (m.status==ChatMessage::Queued ||
        (m.status==ChatMessage::Sent && !channels::isChannel(m.destination)))) {
      m.status=ChatMessage::Failed;persist(m);
    }
  }
}
