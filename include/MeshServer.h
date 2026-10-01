#pragma once
#include <Arduino.h>
namespace mesh {class Radio;class MillisecondClock;class RNG;class RTCClock;class PacketManager;class MeshTables;class LocalIdentity;}
class ServerMesh;
// Server roles: the MeshCore repeater and room server, with stock simple_repeater / simple_room_server
// behaviour (forwarding rules, admin and guest login, remote CLI, status, telemetry; neighbours for the
// repeater, posts for the room) on MeshMesh's radio, clock, storage and screen. The network profile and
// name stay in Config; the server's own settings are MeshCore's /prefs.json.
struct ServerView {
  bool running=false,room=false,forwarding=false,readOnlyLogin=false;
  String password,guest;
  unsigned advertMinutes=0,floodAdvertHours=0,floodMax=0,neighbours=0,posts=0,clients=0,admins=0;
  uint32_t uptime=0,floodRx=0,directRx=0,floodTx=0,directTx=0;
};
class MeshServer {
 public:
  bool begin(uint8_t role,mesh::Radio& radio,mesh::MillisecondClock& ms,mesh::RNG& rng,mesh::RTCClock& rtc,mesh::PacketManager& pool,mesh::MeshTables& tables,const mesh::LocalIdentity& identity);
  bool running() const{return node!=nullptr;}
  bool room() const;
  void tick();
  void configChanged();                 // radio, power or name changed in MeshMesh settings
  bool advertise(bool flood=true);
  bool post(const String& text);        // the room: a post by the room itself, as "room.post"
  String command(const String& line);   // the MeshCore CLI as the local admin (USB, web, screen)
  String json(bool secrets=false);
  ServerView view();
  const mesh::LocalIdentity* identity() const;
 private:
  ServerMesh* node=nullptr;
};
extern MeshServer meshServer;
