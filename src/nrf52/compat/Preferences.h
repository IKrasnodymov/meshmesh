#pragma once
// ESP32 Preferences (NVS) on nRF52: one file per namespace in MeshMesh storage (/prefs/<name>).
// Every put rewrites the file through a temporary copy and a rename, so a reset keeps either the
// old or the new set; put* returns the stored size only after the file is written.
#include <Arduino.h>
class Preferences {
 public:
  ~Preferences(){end();}
  bool begin(const char* name,bool readOnly=false,const char* partition=nullptr);
  void end();
  bool clear();bool remove(const char* key);bool isKey(const char* key);
  size_t putBool(const char* k,bool v){uint8_t b=v;return put(k,&b,1)?1:0;}
  size_t putUChar(const char* k,uint8_t v){return put(k,&v,1)?1:0;}
  size_t putChar(const char* k,int8_t v){return put(k,&v,1)?1:0;}
  size_t putUShort(const char* k,uint16_t v){return put(k,&v,2)?2:0;}
  size_t putShort(const char* k,int16_t v){return put(k,&v,2)?2:0;}
  size_t putUInt(const char* k,uint32_t v){return put(k,&v,4)?4:0;}
  size_t putInt(const char* k,int32_t v){return put(k,&v,4)?4:0;}
  size_t putULong(const char* k,uint32_t v){return putUInt(k,v);}
  size_t putFloat(const char* k,float v){return put(k,&v,4)?4:0;}
  size_t putBytes(const char* k,const void* v,size_t n){return put(k,v,n)?n:0;}
  size_t putString(const char* k,const char* v){size_t n=strlen(v);return put(k,v,n)?n:0;}
  size_t putString(const char* k,const String& v){return putString(k,v.c_str());}
  bool getBool(const char* k,bool d=false){uint8_t v=d;get(k,&v,1);return v;}
  uint8_t getUChar(const char* k,uint8_t d=0){get(k,&d,1);return d;}
  int8_t getChar(const char* k,int8_t d=0){get(k,&d,1);return d;}
  uint16_t getUShort(const char* k,uint16_t d=0){get(k,&d,2);return d;}
  int16_t getShort(const char* k,int16_t d=0){get(k,&d,2);return d;}
  uint32_t getUInt(const char* k,uint32_t d=0){get(k,&d,4);return d;}
  int32_t getInt(const char* k,int32_t d=0){get(k,&d,4);return d;}
  uint32_t getULong(const char* k,uint32_t d=0){return getUInt(k,d);}
  float getFloat(const char* k,float d=0){get(k,&d,4);return d;}
  size_t getBytesLength(const char* k){int i=find(k);return i<0?0:entryLength(i);}
  size_t getBytes(const char* k,void* out,size_t max);
  size_t getString(const char* k,char* out,size_t max);
  String getString(const char* k,const String& d=String());
 private:
  char path[40]={};bool open=false,writable=false;
  uint8_t* data=nullptr;size_t size=0; // records: key length, key, value length (2 bytes LE), value
  int find(const char* key) const;size_t entryLength(int at) const;
  bool get(const char* key,void* out,size_t n);
  bool put(const char* key,const void* value,size_t n);
  bool flush();
};
