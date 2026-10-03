#pragma once
// Host shim: just enough Arduino API to render the production UI on a computer.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string>
#include <algorithm>
#include "Print.h"
class TwoWire;
#define PROGMEM
#define IRAM_ATTR
class __FlashStringHelper;
#define F(s) (s)
inline float radians(float d){return d*float(M_PI)/180;}
inline float degrees(float r){return r*180/float(M_PI);}
#ifndef pgm_read_byte
#define pgm_read_byte(a) (*(const unsigned char*)(a))
#define pgm_read_word(a) (*(const unsigned short*)(a))
#define pgm_read_dword(a) (*(const unsigned long*)(a))
#endif
using std::min;using std::max;
template<class T,class L,class H> auto constrain(T v,L lo,H hi)->decltype(v+lo+hi){return v<lo?lo:v>hi?hi:v;}
uint32_t millis();
inline void delay(uint32_t){}
#define OUTPUT 1
#define LOW 0
#define HIGH 1
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline void yield(){}
inline long random(long a){return rand()%a;}
class String {
  std::string s;
  static std::string num(long long v,int base){if(base==10)return std::to_string(v);std::string r;unsigned long long u=v;do{r.insert(r.begin(),"0123456789abcdefghijklmnopqrstuvwxyz"[u%base]);u/=base;}while(u);return r;}
 public:
  String()=default;
  String(const char* v):s(v?v:""){}
  String(const std::string& v):s(v){}
  String(char c):s(1,c){}
  String(unsigned char v,unsigned char base=10):s(num(v,base)){}
  String(int v,unsigned char base=10):s(num(v,base)){}
  String(unsigned v,unsigned char base=10):s(num(v,base)){}
  String(long v,unsigned char base=10):s(num(v,base)){}
  String(unsigned long v,unsigned char base=10):s(num(v,base)){}
  String(long long v,unsigned char base=10):s(num(v,base)){}
  String(unsigned long long v,unsigned char base=10):s(num((long long)v,base)){}
  String(double v,unsigned int d=2){char b[64];snprintf(b,sizeof b,"%.*f",int(d),v);s=b;}
  String(float v,unsigned int d=2):String(double(v),d){}
  size_t length() const{return s.size();}
  const char* c_str() const{return s.c_str();}
  bool isEmpty() const{return s.empty();}
  char operator[](unsigned i) const{return i<s.size()?s[i]:0;}
  char& operator[](unsigned i){return s[i];}
  char charAt(unsigned i) const{return (*this)[i];}
  String substring(unsigned a) const{return a>=s.size()?String():String(s.substr(a));}
  String substring(unsigned a,unsigned b) const{if(a>b)std::swap(a,b);if(a>=s.size())return String();return String(s.substr(a,std::min<size_t>(b,s.size())-a));}
  bool startsWith(const String& p) const{return s.compare(0,p.s.size(),p.s)==0;}
  bool endsWith(const String& p) const{return s.size()>=p.s.size()&&s.compare(s.size()-p.s.size(),p.s.size(),p.s)==0;}
  int indexOf(char c,unsigned from=0) const{auto r=s.find(c,from);return r==std::string::npos?-1:int(r);}
  int indexOf(const String& v,unsigned from=0) const{auto r=s.find(v.s,from);return r==std::string::npos?-1:int(r);}
  void remove(unsigned i){if(i<s.size())s.erase(i);}
  void remove(unsigned i,unsigned n){if(i<s.size())s.erase(i,n);}
  void replace(const String& a,const String& b){if(a.s.empty())return;size_t at=0;while((at=s.find(a.s,at))!=std::string::npos){s.replace(at,a.s.size(),b.s);at+=b.s.size();}}
  void toLowerCase(){for(auto& c:s)if(c>='A'&&c<='Z')c+=32;}
  void trim(){size_t a=s.find_first_not_of(" \t\r\n");size_t b=s.find_last_not_of(" \t\r\n");s=a==std::string::npos?"":s.substr(a,b-a+1);}
  void reserve(unsigned n){s.reserve(n);}
  long toInt() const{return atol(s.c_str());}
  float toFloat() const{return atof(s.c_str());}
  bool concat(const char* v){s+=v;return true;}
  bool concat(const String& v){s+=v.s;return true;}
  bool concat(char c){s+=c;return true;}
  bool equals(const String& v) const{return s==v.s;}
  String& operator+=(const String& v){s+=v.s;return *this;}
  String& operator+=(const char* v){s+=v;return *this;}
  String& operator+=(char c){s+=c;return *this;}
  bool operator==(const String& v) const{return s==v.s;}
  bool operator==(const char* v) const{return s==v;}
  bool operator!=(const String& v) const{return s!=v.s;}
  bool operator!=(const char* v) const{return s!=v;}
  bool operator<(const String& v) const{return s<v.s;}
  friend String operator+(const String& a,const String& b){return String(a.s+b.s);}
  friend String operator+(const String& a,const char* b){return String(a.s+b);}
  friend String operator+(const char* a,const String& b){return String(a+b.s);}
  friend String operator+(const String& a,char b){return String(a.s+b);}
};
class HardwareSerialShim:public Print{public:size_t write(uint8_t c) override{return fputc(c,stderr)!=EOF;}void begin(unsigned){}int available(){return 0;}int read(){return -1;}};
extern HardwareSerialShim Serial;
class EspShim{public:uint32_t getFreeHeap(){return 187*1024;}uint32_t getFreePsram(){return 7900*1024;}uint32_t getPsramSize(){return 8192*1024;}uint64_t getEfuseMac(){return 0x1234567890ULL;}};
extern EspShim ESP;
