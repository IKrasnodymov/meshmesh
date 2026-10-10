#pragma once
#include "MeshRadio.h"
// The same JSON record for whole replies and bounded streaming on nRF52.
inline void historyJsonRecord(JsonDocument& d,const ChatMessage& m){
 d.clear();JsonObject j=d.to<JsonObject>();j["protocol"]=m.protocol;j["source"]=meshRadio.idText(m.source);j["destination"]=meshRadio.idText(m.destination);j["session"]=m.session;j["id"]=m.id;j["name"]=m.name;j["text"]=m.text;j["time"]=m.timestamp;j["outgoing"]=m.outgoing;j["status"]=int(m.status);
 if(m.route){j["route"]=m.route==ChatMessage::RouteDirect?"direct":"flood";if(m.hops!=255)j["hops"]=m.hops;if(m.tries)j["tries"]=m.tries;}MeshRadio::pathJson(j,m);
}
