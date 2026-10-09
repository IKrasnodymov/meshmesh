#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: region names are not speed-critical (the rest of the nRF52 image is -O2)
#endif
#include "Regions.h"
#include <SHA256.h>
#include <Mm1Packet.h>
namespace regions {
static bool nameChar(uint8_t c){return c=='-'||c=='$'||c=='#'||(c>='0'&&c<='9')||c>='A';} // RegionMap::is_name_char
String normalize(const String& raw){
 String s=raw;s.trim();while(s.startsWith("#"))s.remove(0,1);
 if(!s.length()||s.length()>NameBytes||s[0]=='$'||!meshmesh::validUtf8((const uint8_t*)s.c_str(),s.length()))return "";
 for(unsigned i=0;i<s.length();i++)if(!nameChar(s[i])||s[i]==127)return "";
 return s;
}
void keyOf(const String& name,uint8_t key[16]){String tag="#"+name;SHA256 h;h.update(tag.c_str(),tag.length());h.finalize(key,16);}
String cycle(const String& value,int dir,bool channel){
 String list[MaxFound+3];unsigned n=0;list[n++]="";if(channel)list[n++]=Unscoped;
 for(unsigned i=0;i<search.count;i++)list[n++]=search.found[i].name;
 int at=-1;for(unsigned i=0;i<n;i++)if(list[i]==value)at=i;
 if(at<0){list[n]=value;at=n++;}
 return list[(at+dir+int(n)*2)%n];
}
}
