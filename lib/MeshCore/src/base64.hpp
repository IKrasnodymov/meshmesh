#pragma once
#if defined(ESP32)
#include <mbedtls/base64.h>
inline int decode_base64(unsigned char* src,int len,unsigned char* dest) { size_t written=0;return mbedtls_base64_decode(dest,32,&written,src,len)==0?int(written):0; }
#else
// nRF52: no mbedTLS in the Arduino core. The same contract: at most 32 bytes out, 0 on any error.
inline int decode_base64(unsigned char* src,int len,unsigned char* dest) {
  auto value=[](unsigned char c)->int{return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;};
  if(len%4)return 0;int out=0;
  for(int i=0;i<len;i+=4){
    int pad=(src[i+3]=='=')+(src[i+2]=='=');if(pad&&i+4!=len)return 0;
    int a=value(src[i]),b=value(src[i+1]),c=pad>1?0:value(src[i+2]),d=pad?0:value(src[i+3]);if(a<0||b<0||c<0||d<0)return 0;
    unsigned long v=(unsigned long)a<<18|b<<12|c<<6|d;unsigned char bytes[3]={(unsigned char)(v>>16),(unsigned char)(v>>8),(unsigned char)v};
    for(int k=0;k<3-pad;k++){if(out>=32)return 0;dest[out++]=bytes[k];}
  }
  return out;
}
#endif
