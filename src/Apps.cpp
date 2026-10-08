#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash, as UiHeltec.cpp
#endif
#include "App.h"

// The screen's apps in the chosen order: config.apps lists their IDs separated by spaces, "-" before an ID
// hides it. IDs the text does not name (apps new in this firmware) follow in the default order, shown.
namespace {
int appIndex(const char* id,size_t n){for(uint8_t i=0;i<uiAppCount;i++)if(strlen(uiApps[i])==n&&!strncmp(uiApps[i],id,n))return i;return -1;}
}
bool appsParse(const char* text,uint8_t* order,bool* hidden,bool strict){
  bool seen[AppsMax]={};unsigned n=0;
  for(const char* p=text;*p;){
    while(*p==' '||*p==',')p++;if(!*p)break;
    bool off=*p=='-';if(off)p++;const char* end=p;while(*end&&*end!=' '&&*end!=',')end++;
    int i=appIndex(p,end-p);p=end;
    if(i<0||seen[i]){if(strict)return false;continue;}
    seen[i]=true;order[n++]=i;hidden[i]=off&&strcmp(uiApps[i],"settings"); // Settings stay: the way back to everything else
  }
  for(uint8_t i=0;i<uiAppCount;i++)if(!seen[i]){order[n++]=i;hidden[i]=false;}
  return true;
}
String appsText(const char* text){
  uint8_t order[AppsMax];bool hidden[AppsMax];appsParse(text,order,hidden,false);
  String s;for(uint8_t i=0;i<uiAppCount;i++){if(i)s+=' ';if(hidden[order[i]])s+='-';s+=uiApps[order[i]];}return s;
}
unsigned appsShown(uint8_t* out){
  static char parsed[sizeof config.apps]={1};static uint8_t shown[AppsMax];static unsigned count=0;
  if(strcmp(parsed,config.apps)){
    strlcpy(parsed,config.apps,sizeof parsed);uint8_t order[AppsMax];bool hidden[AppsMax];appsParse(config.apps,order,hidden,false);
    count=0;for(uint8_t i=0;i<uiAppCount;i++)if(!hidden[order[i]])shown[count++]=order[i];
  }
  memcpy(out,shown,count);return count;
}
