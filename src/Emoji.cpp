#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash (AGENTS.md, "Языки")
#endif
#define EMOJI_DATA_DEFINE
#include "Emoji.h"
namespace emoji {
int find(uint32_t cp){
 uint32_t k=cp>=0x1f000&&cp<=0x1ffff?cp-0x10000:cp;if(k>=0xf000&&cp<0x1f000)return -1;
 int lo=0,hi=Count-1;
 while(lo<=hi){int mid=(lo+hi)/2;if(keys[mid]==k)return mid;if(keys[mid]<k)lo=mid+1;else hi=mid-1;}
 return -1;
}
bool decode(int index,uint8_t px[Size*Size]){
 if(index<0||unsigned(index)>=Count)return false;
 unsigned at=0;for(int i=0;i<index;i++)at+=sizes[i];
 const uint8_t* data=bits+at;unsigned end=sizes[index]*8,pos=0;
 auto bit=[&]()->int{if(pos>=end)return -1;int b=data[pos>>3]>>(7-(pos&7))&1;pos++;return b;};
 auto take=[&](unsigned n,uint32_t& v)->bool{v=0;while(n--){int b=bit();if(b<0)return false;v=v<<1|b;}return true;};
 uint32_t k,c;uint8_t colours[15];if(!take(4,k)||!k)return false;
 for(unsigned i=0;i<k;i++){if(!take(5,c)||c>Colors)return false;colours[i]=c;}
 unsigned width=1;while((1u<<width)<k+1)width++;
 for(unsigned n=0;n<Size*Size;){
  uint32_t code,run;if(!take(width,code)||code>k)return false;
  unsigned zeros=0;int b;while((b=bit())==0)zeros++;if(b<0||zeros>8)return false;
  if(!take(zeros,run))return false;run|=1u<<zeros;
  if(n+run>Size*Size||(code==k&&n<Size))return false;
  for(;run;run--,n++)px[n]=code==k?px[n-Size]:colours[code];
 }
 return true;
}
static unsigned luma(uint8_t v){uint32_t c=palette[v-1];return ((c>>16&255)*299+(c>>8&255)*587+(c&255)*114)/1000;}
void mono(uint8_t px[Size*Size]){
 unsigned lo=255,hi=0;for(unsigned i=0;i<Size*Size;i++)if(px[i]){unsigned l=luma(px[i]);if(l<lo)lo=l;if(l>hi)hi=l;}
 if(hi<lo)return;
 if(hi-lo<90){for(unsigned i=0;i<Size*Size;i++)px[i]=px[i]?1:0;return;}
 unsigned mid=(lo+hi)/2,bright=0,dark=0;for(unsigned i=0;i<Size*Size;i++)if(px[i])(luma(px[i])>=mid?bright:dark)++;
 bool lightBright=bright>=dark;
 for(unsigned i=0;i<Size*Size;i++)px[i]=px[i]&&(luma(px[i])>=mid)==lightBright?1:0;
}
}
