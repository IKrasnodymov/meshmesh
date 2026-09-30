#pragma once
#include <mbedtls/base64.h>
inline int decode_base64(unsigned char* src,int len,unsigned char* dest) { size_t written=0;return mbedtls_base64_decode(dest,32,&written,src,len)==0?int(written):0; }
