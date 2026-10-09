#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: channel names, links and keys are not speed-critical (the rest of the nRF52 image is -O2)
#endif
#include "Channels.h"
#include <SHA256.h>
#include <Mm1Packet.h>
#include <ctype.h>
#include <limits.h>
namespace channels {
const uint8_t publicSecret[16]={0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72};
// Names people tend to give open channels; each costs two hashes per heard packet with a matching byte.
const char* const commonTags[]={"#test","#testing","#ping","#bot","#chat","#general","#local","#mesh","#meshcore","#news","#weather",
 "#emergency","#sos","#help","#random","#offtopic","#repeaters","#wardrive","#wardriving","#ru","#russia","#moscow","#msk","#spb",
 "#kazan","#tatarstan","#ua","#ukraine","#kyiv","#de","#germany","#pl","#poland","#uk","#us","#usa","#eu","#europe"};
const unsigned commonTagCount=sizeof(commonTags)/sizeof(commonTags[0]);
static void sha(uint8_t* out,size_t n,const void* data,size_t len){SHA256 h;h.update(data,len);h.finalize(out,n);}
uint64_t idOf(const uint8_t secret[16]){
 if(isPublic(secret))return meshmesh::Broadcast;
 uint8_t d[8];sha(d,8,secret,16);uint64_t id=0xFF;for(int i=0;i<7;i++)id=id<<8|d[i];
 return id==meshmesh::Broadcast?id^1:id;
}
uint8_t hashOf(const uint8_t secret[16]){uint8_t d;sha(&d,1,secret,16);return d;}
bool isPublic(const uint8_t secret[16]){return !memcmp(secret,publicSecret,16);}
String hashtag(const String& raw){
 String s,r=raw;r.trim();unsigned i=0;while(i<r.length()&&r[i]=='#')i++;
 for(;i<r.length();i++){char c=r[i];if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;if(c>='A'&&c<='Z')c+=32;s+=c;}
 if(!s.length()||s.length()>NameBytes-1||!meshmesh::validUtf8((const uint8_t*)s.c_str(),s.length()))return "";
 for(unsigned k=0;k<s.length();k++)if(uint8_t(s[k])<32||s[k]==127)return "";
 return "#"+s;
}
void hashtagSecret(const String& tag,uint8_t out[16]){sha(out,16,tag.c_str(),tag.length());}
bool isHashtag(const Channel& c){if(c.name[0]!='#')return false;uint8_t k[16];hashtagSecret(c.name,k);return !memcmp(k,c.secret,16);}
bool validName(const String& name){
 if(!name.length()||name.length()>NameBytes||!meshmesh::validUtf8((const uint8_t*)name.c_str(),name.length()))return false;
 for(unsigned i=0;i<name.length();i++)if(uint8_t(name[i])<32||name[i]==127)return false;return true;
}
static int hexValue(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static int b64Value(char c){return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'||c=='-'?62:c=='/'||c=='_'?63:-1;}
bool parseKey(String text,uint8_t out[16]){
 text.trim();String s;for(unsigned i=0;i<text.length();i++)if(text[i]!=' ')s+=text[i];
 bool zero=true;
 if(s.length()==32){bool hex=true;for(unsigned i=0;i<32&&hex;i++)hex=hexValue(s[i])>=0;if(hex){for(int i=0;i<16;i++){out[i]=hexValue(s[i*2])<<4|hexValue(s[i*2+1]);zero&=!out[i];}return !zero;}}
 while(s.endsWith("="))s.remove(s.length()-1);
 if(s.length()!=22)return false; // 16 bytes: 22 characters without padding
 uint32_t acc=0;int bits=0;unsigned n=0;
 for(unsigned i=0;i<s.length();i++){int v=b64Value(s[i]);if(v<0)return false;acc=acc<<6|v;bits+=6;if(bits>=8){bits-=8;if(n<16)out[n++]=acc>>bits&0xff;}}
 if(n!=16||(acc&((1u<<bits)-1)))return false;
 for(int i=0;i<16;i++)zero&=!out[i];return !zero;
}
String keyHex(const uint8_t secret[16]){String s;char b[3];for(int i=0;i<16;i++){snprintf(b,3,"%02x",secret[i]);s+=b;}return s;}
static String encode(const char* text,bool compact){
 String s;char b[4];
 for(const char* p=text;*p;p++){char ch=*p;if(isalnum((unsigned char)ch)||ch=='-'||ch=='_'||ch=='.'||ch=='~'||(compact&&uint8_t(ch)>=0x80))s+=ch;else{snprintf(b,4,"%%%02X",uint8_t(ch));s+=b;}}
 return s;
}
String link(const Channel& c,bool compact){
 String s="meshcore://channel/add?name="+encode(c.name,compact)+"&secret="+keyHex(c.secret);
 if(c.region[0]&&strcmp(c.region,"*"))s+="&region_scope="+encode(c.region,compact); // MeshCore app 1.47+
 return s;
}
static String decode(const String& v){
 String out;for(unsigned i=0;i<v.length();i++){char c=v[i];
  if(c=='+')out+=' ';else if(c=='%'&&i+2<v.length()&&hexValue(v[i+1])>=0&&hexValue(v[i+2])>=0){out+=char(hexValue(v[i+1])<<4|hexValue(v[i+2]));i+=2;}else out+=c;}
 return out;
}
static int findLink(const String& text){String low=text;low.toLowerCase();return low.indexOf("meshcore://channel/add?");}
bool hasLink(const String& text){return findLink(text)>=0;}
bool parseLink(const String& text,String& name,uint8_t secret[16],String* region){
 int at=findLink(text);if(at<0)return false;
 int end=at;while(end<int(text.length())&&uint8_t(text[end])>' ')end++;
 String query=text.substring(text.indexOf('?',at)+1,end);bool keyed=false;name="";
 while(query.length()){int amp=query.indexOf('&');String pair=amp<0?query:query.substring(0,amp);query=amp<0?String():query.substring(amp+1);
  int eq=pair.indexOf('=');if(eq<0)continue;String k=pair.substring(0,eq),v=decode(pair.substring(eq+1));
  if(k=="name")name=v;else if(k=="region_scope"&&region)*region=v;else if(k=="secret"){if(v.length()!=32||!parseKey(v,secret))return false;keyed=true;}}
 name.trim();
 if(!validName(name)){ // too long: cut on a character boundary; nothing usable: a generic name
  while(name.length()>NameBytes){unsigned n=meshmesh::previousCharacter(name.c_str(),name.length());name.remove(n);}
  if(!validName(name))name="Channel";
 }
 return keyed;
}

// QR code, ISO/IEC 18004: byte mode, error correction level M, versions 1-10.
namespace {
const int8_t eccPerBlock[11]={0,10,16,26,18,24,16,18,22,22,26},blockCount[11]={0,1,1,1,2,2,4,4,4,5,5};
int rawModules(int v){int n=(16*v+128)*v+64;if(v>=2){int a=v/7+2;n-=(25*a-10)*a-55;if(v>=7)n-=36;}return n;}
struct Grid {
 int size;uint8_t* m;uint8_t fn[QrBytes]={};
 bool get(int x,int y) const{int i=y*size+x;return m[i>>3]>>(i&7)&1;}
 void put(int x,int y,bool dark){int i=y*size+x;if(dark)m[i>>3]|=1<<(i&7);else m[i>>3]&=~(1<<(i&7));}
 bool isFn(int x,int y) const{int i=y*size+x;return fn[i>>3]>>(i&7)&1;}
 void setFn(int x,int y,bool dark){put(x,y,dark);int i=y*size+x;fn[i>>3]|=1<<(i&7);}
};
uint8_t gfMul(uint8_t x,uint8_t y){int z=0;for(int i=7;i>=0;i--){z=(z<<1)^((z>>7)*0x11D);z^=((y>>i)&1)*x;}return z;}
void finder(Grid& g,int cx,int cy){for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++){int d=max(abs(dx),abs(dy)),x=cx+dx,y=cy+dy;if(x>=0&&x<g.size&&y>=0&&y<g.size)g.setFn(x,y,d!=2&&d!=4);}}
void format(Grid& g,int mask){
 int data=0<<3|mask,rem=data;for(int i=0;i<10;i++)rem=(rem<<1)^((rem>>9)*0x537);int bits=(data<<10|rem)^0x5412;auto bit=[&](int i){return (bits>>i&1)!=0;};
 for(int i=0;i<=5;i++)g.setFn(8,i,bit(i));g.setFn(8,7,bit(6));g.setFn(8,8,bit(7));g.setFn(7,8,bit(8));for(int i=9;i<15;i++)g.setFn(14-i,8,bit(i));
 for(int i=0;i<8;i++)g.setFn(g.size-1-i,8,bit(i));for(int i=8;i<15;i++)g.setFn(8,g.size-15+i,bit(i));g.setFn(8,g.size-8,true);
}
bool masked(int mask,int x,int y){
 switch(mask){case 0:return (x+y)%2==0;case 1:return y%2==0;case 2:return x%3==0;case 3:return (x+y)%3==0;case 4:return (x/3+y/2)%2==0;
 case 5:return x*y%2+x*y%3==0;case 6:return (x*y%2+x*y%3)%2==0;default:return ((x+y)%2+x*y%3)%2==0;}
}
void applyMask(Grid& g,int mask){for(int y=0;y<g.size;y++)for(int x=0;x<g.size;x++)if(!g.isFn(x,y)&&masked(mask,x,y))g.put(x,y,!g.get(x,y));}
// Penalty of ISO 18004 8.8.2: runs, 2x2 blocks, finder-like patterns and the dark share.
long penalty(const Grid& g){
 long p=0;int n=g.size,dark=0;
 for(int pass=0;pass<2;pass++)for(int a=0;a<n;a++){
  int run=0;bool last=false;uint32_t window=0;
  for(int b=0;b<n;b++){bool v=pass?g.get(a,b):g.get(b,a);if(!pass&&v)dark++;
   if(b&&v==last){run++;if(run==5)p+=3;else if(run>5)p++;}else{run=1;last=v;}
   window=(window<<1|v)&0x7ff;
   if(b>=10&&(window==0x05d||window==0x5d0))p+=40; // 00001011101 or 10111010000
  }
  // patterns touching the edge: the light quiet zone counts as the missing four
  for(int e=0;e<2;e++){uint32_t w=0;for(int k=0;k<7;k++){int b=e?n-7+k:k;w=w<<1|(pass?g.get(a,b):g.get(b,a));}if(w==0x5d){bool clear=true;for(int k=0;k<4&&clear;k++){int b=e?n-8-k:7+k;clear=!(pass?g.get(a,b):g.get(b,a));}if(clear)p+=40;}}
 }
 for(int y=0;y+1<n;y++)for(int x=0;x+1<n;x++){bool v=g.get(x,y);if(v==g.get(x+1,y)&&v==g.get(x,y+1)&&v==g.get(x+1,y+1))p+=3;}
 int total=n*n,k=(abs(dark*20-total*10)+total-1)/total-1;p+=max(0,k)*10;return p;
}
}
int qr(const String& text,uint8_t* out){
 int len=text.length(),v=1;
 for(;v<=10;v++){int cap=rawModules(v)/8-eccPerBlock[v]*blockCount[v];if(4+(v<10?8:16)+len*8<=cap*8)break;}
 if(v>10)return 0;
 const int size=v*4+17,raw=rawModules(v)/8,ecc=eccPerBlock[v],blocks=blockCount[v],dataLen=raw-ecc*blocks;
 // Data: mode, length, bytes, terminator and padding.
 uint8_t data[400]={};int bit=0;auto push=[&](uint32_t value,int n){for(int i=n-1;i>=0;i--){if(value>>i&1)data[bit>>3]|=0x80>>(bit&7);bit++;}};
 push(4,4);push(len,v<10?8:16);for(int i=0;i<len;i++)push(uint8_t(text[i]),8);
 push(0,min(4,dataLen*8-bit));bit=(bit+7)/8*8;while(bit<dataLen*8){push(0xEC,8);if(bit<dataLen*8)push(0x11,8);}
 // Reed-Solomon per block, then interleaving.
 uint8_t divisor[30]={};divisor[ecc-1]=1;uint8_t root=1;
 for(int i=0;i<ecc;i++){for(int j=0;j<ecc;j++){divisor[j]=gfMul(divisor[j],root);if(j+1<ecc)divisor[j]^=divisor[j+1];}root=gfMul(root,2);}
 uint8_t all[400];int shortBlocks=blocks-raw%blocks,shortLen=raw/blocks;uint8_t ecw[5][30];int starts[5],lens[5],at=0;
 for(int b=0;b<blocks;b++){int n=shortLen-ecc+(b<shortBlocks?0:1);starts[b]=at;lens[b]=n;uint8_t* r=ecw[b];memset(r,0,ecc);
  for(int i=0;i<n;i++){uint8_t f=data[at+i]^r[0];memmove(r,r+1,ecc-1);r[ecc-1]=0;for(int j=0;j<ecc;j++)r[j]^=gfMul(divisor[j],f);}at+=n;}
 int k=0;for(int i=0;i<=shortLen-ecc;i++)for(int b=0;b<blocks;b++)if(i<lens[b])all[k++]=data[starts[b]+i];
 for(int i=0;i<ecc;i++)for(int b=0;b<blocks;b++)all[k++]=ecw[b][i];
 // Function patterns.
 Grid g;g.size=size;g.m=out;memset(out,0,QrBytes);
 for(int i=0;i<size;i++){g.setFn(6,i,i%2==0);g.setFn(i,6,i%2==0);}
 finder(g,3,3);finder(g,size-4,3);finder(g,3,size-4);
 if(v>=2){int count=v/7+2,step=(v*4+count*2+1)/(count*2-2)*2,pos[7];pos[0]=6;for(int i=count-1,p=size-7;i>=1;i--,p-=step)pos[i]=p;
  for(int i=0;i<count;i++)for(int j=0;j<count;j++){if((!i&&!j)||(!i&&j==count-1)||(i==count-1&&!j))continue;
   for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)g.setFn(pos[i]+dx,pos[j]+dy,max(abs(dx),abs(dy))!=1);}}
 format(g,0);
 if(v>=7){int rem=v;for(int i=0;i<12;i++)rem=(rem<<1)^((rem>>11)*0x1F25);long bits=long(v)<<12|rem;
  for(int i=0;i<18;i++){bool d=bits>>i&1;int a=size-11+i%3,b=i/3;g.setFn(a,b,d);g.setFn(b,a,d);}}
 // Codewords in the zigzag order.
 int i=0;for(int right=size-1;right>=1;right-=2){if(right==6)right=5;
  for(int vert=0;vert<size;vert++)for(int j=0;j<2;j++){int x=right-j;bool up=((right+1)&2)==0;int y=up?size-1-vert:vert;
   if(!g.isFn(x,y)&&i<raw*8){g.put(x,y,all[i>>3]>>(7-(i&7))&1);i++;}}}
 // The mask with the lowest penalty.
 int best=0;long bestScore=LONG_MAX;
 for(int m=0;m<8;m++){applyMask(g,m);format(g,m);long s=penalty(g);if(s<bestScore){bestScore=s;best=m;}applyMask(g,m);}
 applyMask(g,best);format(g,best);return size;
}
}
