#pragma once
#include "Arduino.h"
#include <map>
// In-memory NVS for previews; nothing leaves the process.
class Preferences {
  std::string ns;bool ro=true;
  static std::map<std::string,std::string>& store(){static std::map<std::string,std::string> s;return s;}
  std::string k(const char* key) const{return ns+"/"+key;}
 public:
  bool begin(const char* name,bool readOnly=false){ns=name;ro=readOnly;return true;}
  void end(){}
  bool isKey(const char* key){return store().count(k(key));}
  uint32_t getUInt(const char* key,uint32_t d=0){auto i=store().find(k(key));return i==store().end()?d:strtoul(i->second.c_str(),nullptr,10);}
  size_t putUInt(const char* key,uint32_t v){store()[k(key)]=std::to_string(v);return 4;}
  int32_t getInt(const char* key,int32_t d=0){auto i=store().find(k(key));return i==store().end()?d:atol(i->second.c_str());}
  size_t putInt(const char* key,int32_t v){store()[k(key)]=std::to_string(v);return 4;}
  uint8_t getUChar(const char* key,uint8_t d=0){return getUInt(key,d);}
  size_t putUChar(const char* key,uint8_t v){putUInt(key,v);return 1;}
  bool getBool(const char* key,bool d=false){return getUInt(key,d);}
  size_t putBool(const char* key,bool v){putUInt(key,v);return 1;}
  String getString(const char* key,const String& d=String()){auto i=store().find(k(key));return i==store().end()?d:String(i->second);}
  size_t putString(const char* key,const String& v){store()[k(key)]=v.c_str();return v.length();}
  size_t getBytesLength(const char* key){auto i=store().find(k(key));return i==store().end()?0:i->second.size();}
  size_t getBytes(const char* key,void* b,size_t n){auto i=store().find(k(key));if(i==store().end())return 0;n=std::min(n,i->second.size());memcpy(b,i->second.data(),n);return n;}
  size_t putBytes(const char* key,const void* b,size_t n){store()[k(key)]=std::string((const char*)b,n);return n;}
  bool remove(const char* key){return store().erase(k(key));}
};
