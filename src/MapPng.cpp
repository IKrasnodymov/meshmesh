#include "Maps.h"
#if !defined(MM_HELTEC_V4)
#include <esp_heap_caps.h>
#include <esp32s3/rom/miniz.h>
// Web map tiles: non-interlaced PNG, 256 or 512 px square, any colour type.
// 512 px ("retina") tiles are decimated 2:1. Transparency is blended onto
// the same light grey the map uses for missing tiles.
namespace {
uint32_t be32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
uint8_t paeth(int a,int b,int c){int p=a+b-c,pa=abs(p-a),pb=abs(p-b),pc=abs(p-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;}
uint16_t rgb565(unsigned r,unsigned g,unsigned b){return (r&0xf8)<<8|(g&0xfc)<<3|b>>3;}
unsigned blend(unsigned c,unsigned a,unsigned bg){return (c*a+bg*(255-a)+127)/255;}
}
bool decodePngTile(const uint8_t* png,size_t size,uint16_t* out,String& error){
  static const uint8_t signature[8]={137,80,78,71,13,10,26,10};
  if(size<45||memcmp(png,signature,8)){error="Not a PNG image";return false;}
  uint32_t width=0,height=0;uint8_t depth=0,type=0;bool header=false,end=false;
  uint8_t palette[256][3]={},alpha[256];memset(alpha,255,sizeof alpha);size_t packed=0;
  for(size_t at=8;at+12<=size;){
    uint32_t length=be32(png+at);const uint8_t* kind=png+at+4;const uint8_t* body=png+at+8;
    if(length>size-at-12){error="Truncated PNG";return false;}
    if(!memcmp(kind,"IHDR",4)){if(length!=13){error="Bad PNG header";return false;}width=be32(body);height=be32(body+4);depth=body[8];type=body[9];
      if(body[10]||body[11]||body[12]){error="Interlaced PNG";return false;}header=true;}
    else if(!memcmp(kind,"PLTE",4))for(unsigned i=0;i<length/3&&i<256;i++)memcpy(palette[i],body+i*3,3);
    else if(!memcmp(kind,"tRNS",4)&&type==3)for(unsigned i=0;i<length&&i<256;i++)alpha[i]=body[i];
    else if(!memcmp(kind,"IDAT",4))packed+=length;
    else if(!memcmp(kind,"IEND",4)){end=true;break;}
    at+=12+length;
  }
  if(!header||!end||!packed){error="Incomplete PNG";return false;}
  if(width!=height||(width!=256&&width!=512)){error="Tile is not 256 or 512 px";return false;}
  unsigned channels=type==0?1:type==2?3:type==3?1:type==4?2:type==6?4:0;
  bool depthOk=depth==8||(depth==16&&type!=3)||((depth==1||depth==2||depth==4)&&(type==0||type==3));
  if(!channels||!depthOk){error="Unsupported PNG colour format";return false;}
  unsigned bits=channels*depth,stride=(width*bits+7)/8,step=max(1u,bits/8);size_t rawSize=size_t(height)*(stride+1);
  uint8_t* idat=(uint8_t*)heap_caps_malloc(packed,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  uint8_t* raw=(uint8_t*)heap_caps_malloc(rawSize,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  auto* codec=(tinfl_decompressor*)heap_caps_malloc(sizeof(tinfl_decompressor),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  bool ok=idat&&raw&&codec;if(!ok)error="PNG RAM allocation";
  if(ok){
    size_t n=0;for(size_t at=8;at+12<=size;){uint32_t length=be32(png+at);if(!memcmp(png+at+4,"IDAT",4)){memcpy(idat+n,png+at+8,length);n+=length;}if(!memcmp(png+at+4,"IEND",4))break;at+=12+length;}
    tinfl_init(codec);size_t inSize=packed,outSize=rawSize;
    tinfl_status status=tinfl_decompress(codec,idat,&inSize,raw,raw,&outSize,TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    ok=status==TINFL_STATUS_DONE&&outSize==rawSize;if(!ok)error="Damaged PNG data";
  }
  free(idat);free(codec);
  const unsigned scale=width/256,bgR=0xe0,bgG=0xe4,bgB=0xe0,maxGray=(1u<<min<unsigned>(depth,8))-1;
  for(uint32_t y=0;ok&&y<height;y++){
    uint8_t* line=raw+y*(stride+1)+1;const uint8_t* prev=y?line-(stride+1):nullptr;uint8_t filter=line[-1];
    if(filter>4){ok=false;error="Bad PNG filter";break;}
    for(unsigned i=0;i<stride;i++){
      unsigned a=i>=step?line[i-step]:0,b=prev?prev[i]:0,c=prev&&i>=step?prev[i-step]:0;
      line[i]+=filter==1?a:filter==2?b:filter==3?(a+b)>>1:filter==4?paeth(a,b,c):0;
    }
    if(y%scale)continue;
    uint16_t* row=out+(y/scale)*256;
    for(unsigned ox=0;ox<256;ox++){
      unsigned x=ox*scale;auto sample=[&](unsigned ch)->unsigned{
        if(depth==8)return line[x*channels+ch];if(depth==16)return line[(x*channels+ch)*2];
        unsigned bit=x*depth;return (line[bit/8]>>(8-depth-bit%8))&maxGray;};
      unsigned r,g,b,alphaValue=255;
      if(type==3){unsigned i=sample(0);r=palette[i][0];g=palette[i][1];b=palette[i][2];alphaValue=alpha[i];}
      else if(type==0||type==4){unsigned v=sample(0);if(depth<8)v=v*255/maxGray;r=g=b=v;if(type==4)alphaValue=sample(1);}
      else{r=sample(0);g=sample(1);b=sample(2);if(type==6)alphaValue=sample(3);}
      if(alphaValue<255){r=blend(r,alphaValue,bgR);g=blend(g,alphaValue,bgG);b=blend(b,alphaValue,bgB);}
      row[ox]=rgb565(r,g,b);
    }
  }
  free(raw);return ok;
}
#endif
