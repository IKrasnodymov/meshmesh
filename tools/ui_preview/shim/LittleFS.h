#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"
namespace previewFs {inline unsigned renameFailures=0;inline std::map<std::string,std::vector<uint8_t>>& files(){static std::map<std::string,std::vector<uint8_t>> value;return value;}}
class File {
 std::string name;size_t at=0;bool open=false,writeable=false;
 public:
 File()=default;File(const char* path,const char* mode):name(path),open(true),writeable(*mode!='r'){
  if(*mode=='r'&&!previewFs::files().count(name)){open=false;return;}auto& bytes=previewFs::files()[name];if(*mode=='w')bytes.clear();if(*mode=='a')at=bytes.size();}
 explicit operator bool() const{return open;}
 size_t size() const{return previewFs::files()[name].size();}
 size_t read(uint8_t* out,size_t count){if(!open)return 0;auto& data=previewFs::files()[name];count=std::min(count,data.size()-at);memcpy(out,data.data()+at,count);at+=count;return count;}
 size_t write(const uint8_t* bytes,size_t count){if(!open||!writeable)return 0;auto& data=previewFs::files()[name];data.resize(std::max(data.size(),at+count));memcpy(data.data()+at,bytes,count);at+=count;return count;}
 size_t write(uint8_t byte){return write(&byte,1);}
 size_t print(const String& value){return write((const uint8_t*)value.c_str(),value.length());}
 size_t println(const String& value){return print(value)+write(uint8_t('\n'));}
 bool available() const{return open&&at<size();}
 String readStringUntil(char delimiter){String value;uint8_t c;while(read(&c,1)){if(c==uint8_t(delimiter))break;value+=char(c);}return value;}
 void close(){open=false;}
};
class PreviewLittleFs {public:File open(const char* path,const char* mode){return File(path,mode);}bool mkdir(const char*){return true;}bool exists(const char* path){return previewFs::files().count(path);}bool remove(const char* path){return previewFs::files().erase(path);}bool rename(const char* a,const char* b){if(previewFs::renameFailures){previewFs::renameFailures--;return false;}if(!exists(a))return false;previewFs::files()[b]=previewFs::files()[a];remove(a);return true;}};
inline PreviewLittleFs LittleFS;
