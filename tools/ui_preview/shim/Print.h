#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
class String;
class Print {
 public:
  virtual ~Print()=default;
  virtual size_t write(uint8_t)=0;
  virtual size_t write(const uint8_t* b,size_t n){size_t r=0;while(n--)r+=write(*b++);return r;}
  size_t write(const char* s){return write((const uint8_t*)s,strlen(s));}
  size_t print(const char* s){return write(s);}
  size_t print(const String& s);
  size_t print(char c){return write(uint8_t(c));}
  size_t println(){return write((const uint8_t*)"\r\n",2);}
  size_t println(const char* s){return print(s)+println();}
};
