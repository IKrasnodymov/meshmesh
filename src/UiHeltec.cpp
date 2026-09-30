#include "App.h"
#include "Hardware.h"
#include "MeshRadio.h"
namespace {int page=0;uint32_t drawAt=0;bool dirty=true;String action;uint32_t actionAt=0;}
namespace {
String clipped(const String& value,unsigned chars){unsigned i=0,n=0;while(i<value.length()&&n<chars){uint8_t c=value[i];i+=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;n++;}return value.substring(0,i);}
String state(const ChatMessage& m){const char* en[]={"Received","Queued","Sent","Delivered","No ACK"};const char* ru[]={"Получено","В очереди","Отправлено","Доставлено","Нет ACK"};return config.russian?ru[m.status]:en[m.status];}
String t(const char* en,const char* ru){return config.russian?ru:en;}
void notice(const String& value){action=value;actionAt=millis();}
void textLines(const String& value,int y,unsigned maximum){String rows[153];unsigned at=0,count=0;while(at<value.length()&&count<153){String row;unsigned n=0;while(at<value.length()&&n<21){uint8_t c=value[at];unsigned bytes=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;at+=bytes;if(c=='\n')break;row+=value.substring(at-bytes,at);n++;}rows[count++]=row;}unsigned first=count>maximum?(millis()/4000)%(count-maximum+1):0;for(unsigned i=first;i<count&&i<first+maximum;i++)hardware.line(y+(i-first)*14,rows[i]);}

}
void uiKey(int key){if(key==13||key==0x82)page=(page+1)%6;
 if(key==0xa3){if(page==3)portalToggle();else if(page==4)bleToggle();else if(page==1&&meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];uint64_t to=m.outgoing?m.destination:m.source;bool sent=to!=meshRadio.nodeId&&meshRadio.sendMessage("OK",to);notice(sent?t("OK reply queued","Ответ OK в очереди"):t("Reply not queued","Ответ не отправлен"));}else if(page==5){bool valid=meshRadio.selfTest();meshRadio.event=valid?"Encryption test OK":"Encryption test FAILED";notice(valid?t("Encryption: OK","Шифрование: OK"):t("Encryption: ERROR","Шифрование: ошибка"));}else notice(meshRadio.sendHello()?t("Node announced","Узел объявлен"):t("Announcement failed","Объявление не отправлено"));}dirty=true;}

static void draw(){hardware.canvas->fillScreen(0);String title[]={"MeshMesh / V4",t("Chat: hold for OK","Чат: держать OK"),t("Nodes","Узлы"),"Wi-Fi", "Bluetooth",t("Modules","Модули")};hardware.line(12,title[page]);hardware.text(109,12,String(page+1)+"/6");
 if(page==0){hardware.line(27,meshRadio.ready?String(config.frequency,3)+" MHz · SF"+String(config.sf):"Radio error "+String(meshRadio.radioError));hardware.line(42,"RX "+String(meshRadio.rxCount)+" TX "+String(meshRadio.txCount)+" · "+String(hardware.batteryMv/1000.f,2)+"V");hardware.line(57,t("PRG: next; hold: ID","PRG: далее; держ: ID"));}
 else if(page==1){if(meshRadio.historyCount){auto& m=meshRadio.history[meshRadio.historyCount-1];hardware.line(27,clipped(m.name,8)+" "+state(m));textLines(m.text,42,2);}else hardware.line(40,t("No messages yet","Сообщений ещё нет"));}
 else if(page==2){if(meshRadio.peerCount){auto& p=meshRadio.peers[0];hardware.line(27,clipped(p.name,21));hardware.line(42,String(int(p.rssi))+" dBm · "+String((millis()-p.seen)/1000)+"s");}else hardware.line(34,t("No nodes heard","Узлы пока не найдены"));hardware.line(57,t("Hold: announce node","Держать: объявить узел"));}
 else if(page==3){hardware.line(27,portalActive()?"MM-"+meshRadio.idText(meshRadio.nodeId).substring(6):t("Hold: enable Wi-Fi","Держать: вкл Wi-Fi"));hardware.line(42,portalActive()?portalPassword():t("Chat in web browser","Чат в веб-браузере"));hardware.line(57,portalActive()?"192.168.4.1":t("Button: next","Кнопка: далее"));}
 else if(page==4){hardware.line(27,bleActive()?"MeshMesh "+meshRadio.idText(meshRadio.nodeId).substring(6):t("Hold: enable BLE","Держать: включить BLE"));hardware.line(42,bleActive()?"PIN "+String(blePin()):t("Secure pairing","Защищённое сопряжение"));hardware.line(57,t("Button: next","Кнопка: далее"));}
 else{hardware.line(27,"LoRa "+String(meshRadio.ready?"OK":"ERR")+" / FS "+String(hardware.fsOk?"OK":"ERR"));hardware.line(42,String(ESP.getFreeHeap()/1024)+"K RAM · "+String(ESP.getFreePsram()/1024)+"K PSRAM");hardware.line(57,t("Hold: encryption test","Держать: проверка AES"));}if(action.length()&&millis()-actionAt<3500){hardware.canvas->fillRect(0,45,128,19,0);hardware.line(57,clipped(action,21));}hardware.flush();}
void uiBegin(){draw();}
String uiStatus(){const char* names[]={"home","messages","nodes","wifi","ble","modules"};StaticJsonDocument<256>d;d["action"]=millis()-actionAt<3500?action:String();d["page"]=names[page];d["locked"]=false;String s;serializeJson(d,s);return s;}
void uiTick(){if((dirty||meshRadio.dirty||millis()-drawAt>1500)&&millis()-drawAt>150){draw();drawAt=millis();dirty=false;meshRadio.dirty=false;}}
