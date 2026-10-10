#pragma once
// Two verified slots: writing the inactive slot leaves the previous setting recoverable.
#include <LittleFS.h>
#include "Hardware.h"
#include <memory>
#include <new>
namespace blob {
inline uint32_t checksum(const void* bytes,size_t n,uint32_t h=2166136261u){const auto* p=static_cast<const uint8_t*>(bytes);while(n--)h=(h^*p++)*16777619u;return h;}
template<class T> struct Record {uint32_t magic=0,generation=0,check=0;T data{};};
template<class T> uint32_t recordChecksum(const Record<T>& r){uint32_t h=checksum(&r.magic,sizeof r.magic);h=checksum(&r.generation,sizeof r.generation,h);return checksum(&r.data,sizeof r.data,h);}
template<class T> bool read(const String& path,uint32_t magic,Record<T>& r){
 File f=LittleFS.open(path.c_str(),FILE_READ);if(!f)return false;bool ok=f.size()==sizeof r&&f.read(reinterpret_cast<uint8_t*>(&r),sizeof r)==sizeof r;f.close();
 return ok&&r.magic==magic&&r.check==recordChecksum(r);
}
template<class T> bool latest(const char* path,uint32_t magic,Record<T>& out){
 std::unique_ptr<Record<T>> other(new(std::nothrow) Record<T>());if(!other)return false;
 bool x=read(String(path)+".0",magic,out),y=read(String(path)+".1",magic,*other);if(!x&&!y)return false;
 if(!x||(y&&int32_t(other->generation-out.generation)>0))out=*other;return true;
}
template<class T> bool load(const char* path,uint32_t magic,T& out,uint32_t* generation=nullptr){
 std::unique_ptr<Record<T>> r(new(std::nothrow) Record<T>());if(!r||!latest(path,magic,*r))return false;
 out=r->data;if(generation)*generation=r->generation;return true;
}
template<class T> bool save(const char* path,uint32_t magic,const T& value){
 if(!hardware.fsOk)return false;std::unique_ptr<Record<T>> r(new(std::nothrow) Record<T>());if(!r)return false;
 bool found=latest(path,magic,*r);uint32_t generation=found?r->generation:0;
 r->magic=magic;r->generation=generation+1;r->data=value;r->check=recordChecksum(*r);String target=String(path)+"."+String(r->generation&1);
 LittleFS.mkdir("/meshmesh");File f=LittleFS.open(target.c_str(),FILE_WRITE);if(!f)return false;
 uint32_t expected=r->check;bool ok=f.write(reinterpret_cast<const uint8_t*>(r.get()),sizeof *r)==sizeof *r;f.close();
 return ok&&read(target,magic,*r)&&r->generation==generation+1&&r->check==expected;
}
}
