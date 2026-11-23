#pragma once
#include "Config.h"
// Regional radio presets: the community list of the MeshCore app (api.meshcore.nz/api/v1/config,
// suggested_radio_settings, checked 2026-11-23). A preset sets frequency, bandwidth, SF and CR, and the path hash
// size when the list gives one; the screens offer only the presets the board's radio takes (Config::valid).
namespace radiopresets {
// hash: path hash bytes of the network (network_settings.path_hash_size), 0 - not given (the setting stays).
struct Preset {const char* name;float frequency,bandwidth;uint8_t sf,cr,hash;};
inline constexpr Preset list[]={
 {"Australia",915.800f,250,10,5,2},
 {"Australia (Narrow)",916.575f,62.5f,7,7,2},
 {"Australia (Mid)",915.075f,125,9,5,2},
 {"Australia: SA, WA",923.125f,62.5f,8,8,2},
 {"Australia: QLD",923.125f,62.5f,8,5,2},
 {"Brazil",923.125f,62.5f,8,8,0},
 {"Canada",910.525f,62.5f,7,5,3},
 {"Costa Rica",910.525f,125,11,5,0},
 {"EU/UK (Narrow)",869.618f,62.5f,8,8,0},
 {"EU/UK (Deprecated)",869.525f,250,11,5,0},
 {"Czech Republic (Narrow)",869.432f,62.5f,7,5,2},
 {"EU 433MHz (Long Range)",433.650f,250,11,5,0},
 {"EU 433MHz (Narrow)",433.650f,62.5f,8,8,0},
 {"Hungary",869.618f,62.5f,7,5,2},
 {"Netherlands",869.618f,62.5f,7,5,0},
 {"Netherlands (Limburg)",869.618f,62.5f,8,8,2},
 {"New Zealand (Narrow)",917.375f,62.5f,7,5,2},
 {"New Zealand (Gisborne)",917.375f,250,11,5,1},
 {"Portugal 433",433.375f,62.5f,9,6,0},
 {"Portugal 868",869.618f,62.5f,7,6,0},
 {"Slovakia",869.618f,62.5f,7,5,2},
 {"Switzerland",869.618f,62.5f,8,8,0},
 {"Thailand",921.000f,62.5f,7,5,0},
 {"USA",910.525f,62.5f,7,5,0},
 {"USA - Southern California",927.875f,62.5f,7,5,3},
 {"Vietnam (Narrow)",920.250f,62.5f,8,5,0},
 {"Vietnam (Deprecated)",920.250f,250,11,5,0},
};
constexpr int Count=sizeof list/sizeof list[0];
inline void apply(Config& c,int i){const Preset& p=list[i];strlcpy(c.preset,p.name,sizeof c.preset);c.frequency=p.frequency;c.bandwidth=p.bandwidth;c.sf=p.sf;c.cr=p.cr;if(p.hash)c.pathHash=p.hash;}
inline bool usable(const Config& c,int i){Config t=c;apply(t,i);return t.valid();}
inline bool fits(const Config& c,int i){const Preset& p=list[i];return fabsf(c.frequency-p.frequency)<.0005f&&c.bandwidth==p.bandwidth&&c.sf==p.sf&&c.cr==p.cr&&(!p.hash||c.pathHash==p.hash);}
inline int find(const char* name){for(int i=0;i<Count;i++)if(!strcmp(list[i].name,name))return i;return -1;}
// The preset the settings are, -1 when none. Several share the radio (EU/UK (Narrow), Switzerland, Netherlands
// (Limburg)): the one chosen last (Config::preset) when it fits, else the first with the same path hash, else the first.
inline int match(const Config& c){
 int hint=find(c.preset);if(hint>=0&&fits(c,hint))return hint;
 for(int i=0;i<Count;i++)if(list[i].hash==c.pathHash&&fits(c,i))return i;
 for(int i=0;i<Count;i++)if(fits(c,i))return i;
 return -1;
}
// The next (dir>0) or previous usable preset from the current one; -1 when the board takes none.
inline int step(const Config& c,int dir){
 int at=match(c);if(at<0)at=dir>0?-1:Count;
 for(int n=0;n<Count;n++){at=(at+(dir>0?1:-1)+Count)%Count;if(usable(c,at))return at;}
 return -1;
}
}
